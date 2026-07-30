#include "UeremcpAbilityTableMutator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "JsonObjectConverter.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "UeremcpContentHash.h"
#include "UeremcpSpellPlanner.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"

namespace
{
using UE::ToolsetRegistry::FGlobalSandbox;

bool NormalizeRow(
	const UScriptStruct* RowStruct,
	const uint8* RowData,
	TSharedPtr<FJsonObject>& OutJson,
	FString& OutRevision,
	FString& OutError)
{
	OutJson = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(
		RowStruct,
		RowData,
		OutJson.ToSharedRef(),
		0,
		0))
	{
		OutError = TEXT("failed to normalize FREAbilityDef row after conversion");
		return false;
	}
	OutRevision = FUeremcpContentHash::HashJsonObject(OutJson, &OutError);
	return !OutRevision.IsEmpty();
}
}

bool FUeremcpAbilityTableMutator::Execute(
	const FUeremcpAbilityTableWritePlan& WritePlan,
	const FUeremcpSpellPlan& SpellPlan,
	FUeremcpAbilityTableMutationResult& OutResult,
	FString& OutError)
{
	OutResult = FUeremcpAbilityTableMutationResult();
	OutError.Reset();

	if (WritePlan.bDryRun)
	{
		OutError = TEXT("dry_run must not enter the DataTable mutator");
		return false;
	}
	if (!WritePlan.bAtomic || !WritePlan.bRollbackOnFailure)
	{
		OutError = TEXT("create_spell mutation currently requires atomic=true and rollback_on_failure=true");
		return false;
	}
	if (!WritePlan.bSave || !WritePlan.bValidate)
	{
		OutError = TEXT("create_spell mutation currently requires save=true and validate=true");
		return false;
	}
	if (!SpellPlan.RowPayload.IsValid())
	{
		OutError = TEXT("spell row payload is missing");
		return false;
	}
	if (FGlobalSandbox::IsActive())
	{
		OutError = FString::Printf(
			TEXT("foreign FileSandbox '%s' is already active"),
			*FGlobalSandbox::GetActiveName());
		return false;
	}

	UScriptStruct* RowStruct = LoadObject<UScriptStruct>(
		nullptr,
		*WritePlan.RowStructPath);
	if (!RowStruct || !RowStruct->IsChildOf(FTableRowBase::StaticStruct()))
	{
		OutError = FString::Printf(
			TEXT("row struct '%s' is unavailable or is not a TableRowBase"),
			*WritePlan.RowStructPath);
		return false;
	}

	FStructOnScope DesiredRow(RowStruct);
	FText ConversionFailure;
	if (!FJsonObjectConverter::JsonObjectToUStruct(
		SpellPlan.RowPayload.ToSharedRef(),
		RowStruct,
		DesiredRow.GetStructMemory(),
		0,
		0,
		true,
		&ConversionFailure))
	{
		OutError = FString::Printf(
			TEXT("FREAbilityDef conversion failed: %s"),
			*ConversionFailure.ToString());
		return false;
	}

	TSharedPtr<FJsonObject> DesiredNormalized;
	FString DesiredRevision;
	if (!NormalizeRow(
		RowStruct,
		DesiredRow.GetStructMemory(),
		DesiredNormalized,
		DesiredRevision,
		OutError))
	{
		return false;
	}

	UDataTable* Table = Cast<UDataTable>(StaticLoadObject(
		UDataTable::StaticClass(),
		nullptr,
		*WritePlan.TableObjectPath,
		nullptr,
		LOAD_NoWarn));
	const bool bPackageWasDirty = Table && Table->GetPackage()->IsDirty();
	if (Table && Table->GetRowStruct() != RowStruct)
	{
		OutError = FString::Printf(
			TEXT("existing DataTable row struct is '%s', expected '%s'"),
			Table->GetRowStruct() ? *Table->GetRowStruct()->GetPathName() : TEXT("<null>"),
			*WritePlan.RowStructPath);
		return false;
	}

	const FName RowName(*WritePlan.RowName);
	const uint8* ExistingRow = Table ? Table->FindRowUnchecked(RowName) : nullptr;
	FStructOnScope ExistingBackup(RowStruct);
	if (ExistingRow)
	{
		RowStruct->CopyScriptStruct(ExistingBackup.GetStructMemory(), ExistingRow);
		TSharedPtr<FJsonObject> ExistingNormalized;
		if (!NormalizeRow(
			RowStruct,
			ExistingRow,
			ExistingNormalized,
			OutResult.RevisionBefore,
			OutError))
		{
			return false;
		}
	}

	if (WritePlan.Mode == TEXT("create") && ExistingRow)
	{
		OutError = FString::Printf(
			TEXT("row '%s' already exists and mode=create forbids replacement"),
			*WritePlan.RowName);
		return false;
	}
	if (WritePlan.bHasExpectedRevision
		&& WritePlan.ExpectedRevision != OutResult.RevisionBefore
		&& WritePlan.OnRevisionConflict != TEXT("force")
		&& WritePlan.OnRevisionConflict != TEXT("replace"))
	{
		OutError = FString::Printf(
			TEXT("expected_revision conflict for row '%s'"),
			*WritePlan.RowName);
		return false;
	}
	if (ExistingRow && OutResult.RevisionBefore == DesiredRevision)
	{
		OutResult.bNoChange = true;
		OutResult.RevisionAfter = DesiredRevision;
		OutResult.bRereadAfterWrite = true;
		return true;
	}

	FString SandboxRequestToken = WritePlan.RequestId;
	for (TCHAR& Character : SandboxRequestToken)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
		{
			Character = TEXT('_');
		}
	}
	const FString SandboxName = FString::Printf(
		TEXT("UEREMCP_create_spell_%s"),
		*SandboxRequestToken);
	if (!FGlobalSandbox::Enter(
		SandboxName,
		TEXT("UEREMCP create_spell FREAbilityDef row mutation")))
	{
		OutError = TEXT("failed to enter FileSandbox");
		return false;
	}

	bool bSandboxEntered = true;
	bool bPersisted = false;
	bool bMutationApplied = false;
	bool bCreatedTable = false;
	ON_SCOPE_EXIT
	{
		if (bSandboxEntered)
		{
			if (!bPersisted)
			{
				if (bMutationApplied && Table && !bCreatedTable)
				{
					if (ExistingRow)
					{
						Table->AddRow(RowName, ExistingBackup.GetStructMemory(), RowStruct);
					}
					else
					{
						Table->RemoveRow(RowName);
					}
					Table->GetPackage()->SetDirtyFlag(bPackageWasDirty);
				}
				if (FGlobalSandbox::Discard())
				{
					OutResult.bRolledBack = true;
				}
			}
			FGlobalSandbox::Leave();
			if (!bPersisted && bCreatedTable && Table)
			{
				FAssetRegistryModule::AssetDeleted(Table);
				Table->ClearFlags(RF_Public | RF_Standalone);
				Table->MarkAsGarbage();
			}
		}
	};

	UPackage* Package = nullptr;
	if (!Table)
	{
		Package = CreatePackage(*WritePlan.TablePackagePath);
		if (!Package)
		{
			OutError = TEXT("failed to create DataTable package");
			return false;
		}
		const FString AssetName =
			FPackageName::GetLongPackageAssetName(WritePlan.TablePackagePath);
		Table = NewObject<UDataTable>(
			Package,
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Table)
		{
			OutError = TEXT("failed to allocate DataTable asset");
			return false;
		}
		Table->RowStruct = RowStruct;
		FAssetRegistryModule::AssetCreated(Table);
		bCreatedTable = true;
		OutResult.bCreatedTable = true;
	}
	else
	{
		Package = Table->GetPackage();
		OutResult.bModifiedTable = true;
	}

	Table->Modify();
	Table->AddRow(RowName, DesiredRow.GetStructMemory(), RowStruct);
	Package->MarkPackageDirty();
	bMutationApplied = true;

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		WritePlan.TablePackagePath,
		FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(
		Package,
		Table,
		*PackageFilename,
		SaveArgs))
	{
		OutError = TEXT("UPackage::SavePackage failed for ability DataTable");
		return false;
	}
	OutResult.bSaved = true;

	const uint8* RereadRow = Table->FindRowUnchecked(RowName);
	TSharedPtr<FJsonObject> RereadNormalized;
	if (!RereadRow
		|| !NormalizeRow(
			RowStruct,
			RereadRow,
			RereadNormalized,
			OutResult.RevisionAfter,
			OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("saved ability row was absent during re-read");
		}
		return false;
	}
	OutResult.bRereadAfterWrite =
		OutResult.RevisionAfter == DesiredRevision;
	if (!OutResult.bRereadAfterWrite)
	{
		OutError = TEXT("normalized ability row did not match after save and re-read");
		return false;
	}

	const TArray<UE::FileSandboxCore::FSandboxedFileChangeInfo> Changes =
		FGlobalSandbox::GetChanges();
	bool bPackageObserved = false;
	for (const UE::FileSandboxCore::FSandboxedFileChangeInfo& Change : Changes)
	{
		OutResult.SandboxedFiles.Add(Change.Path);
		bPackageObserved =
			bPackageObserved || FPaths::IsSamePath(Change.Path, PackageFilename);
	}
	if (!bPackageObserved)
	{
		OutError = TEXT("FileSandbox did not report the saved DataTable package");
		return false;
	}
	if (!FGlobalSandbox::Persist(OutResult.SandboxedFiles))
	{
		OutError = TEXT("FileSandbox persist failed for ability DataTable");
		return false;
	}
	bPersisted = true;
	OutResult.bPersisted = true;
	if (!FGlobalSandbox::Leave())
	{
		OutError = TEXT("DataTable persisted, but FileSandbox leave failed");
		return false;
	}
	bSandboxEntered = false;
	return true;
}
