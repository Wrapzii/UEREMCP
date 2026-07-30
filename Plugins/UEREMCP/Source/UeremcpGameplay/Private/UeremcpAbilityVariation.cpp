#include "UeremcpAbilityVariation.h"

#include "Engine/DataTable.h"
#include "JsonObjectConverter.h"
#include "Misc/PackageName.h"
#include "UeremcpContentHash.h"
#include "UObject/UObjectGlobals.h"

namespace
{
const TCHAR* AbilityRowStructPath = TEXT("/Script/RE.REAbilityDef");

bool IsStableRowName(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64 || !FChar::IsAlpha(Value[0]))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

FString ToObjectPath(const FString& PackagePath)
{
	if (PackagePath.Contains(TEXT(".")))
	{
		return PackagePath;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	return FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
}

bool ReadRow(
	const FString& TablePath,
	const FString& RowName,
	TSharedPtr<FJsonObject>& OutRow,
	FString& OutError)
{
	UDataTable* Table = Cast<UDataTable>(StaticLoadObject(
		UDataTable::StaticClass(),
		nullptr,
		*ToObjectPath(TablePath),
		nullptr,
		LOAD_NoWarn));
	if (!Table)
	{
		OutError = FString::Printf(TEXT("ability_table '%s' could not be loaded"), *TablePath);
		return false;
	}
	const UScriptStruct* RowStruct = Table->GetRowStruct();
	if (!RowStruct || RowStruct->GetPathName() != AbilityRowStructPath)
	{
		OutError = FString::Printf(
			TEXT("ability_table '%s' does not use /Script/RE.REAbilityDef"),
			*TablePath);
		return false;
	}
	const uint8* RowData = Table->FindRowUnchecked(FName(*RowName));
	if (!RowData)
	{
		OutError = FString::Printf(
			TEXT("source_row '%s' was not found in '%s'"),
			*RowName,
			*TablePath);
		return false;
	}
	OutRow = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(
		RowStruct,
		RowData,
		OutRow.ToSharedRef(),
		0,
		0))
	{
		OutError = FString::Printf(TEXT("failed to normalize source_row '%s'"), *RowName);
		return false;
	}
	return true;
}

TSharedPtr<FJsonObject> SelectProtectedFields(
	const TSharedPtr<FJsonObject>& Row,
	const TArray<FString>& Fields,
	FString& OutError)
{
	const TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	for (const FString& Field : Fields)
	{
		const TSharedPtr<FJsonValue> Value = Row.IsValid() ? Row->TryGetField(Field) : nullptr;
		if (!Value.IsValid())
		{
			OutError = FString::Printf(
				TEXT("FREAbilityDef protected field '%s' is absent"),
				*Field);
			return nullptr;
		}
		Snapshot->SetField(Field, Value);
	}
	return Snapshot;
}
}

bool FUeremcpAbilityVariation::BuildPlan(
	const TSharedPtr<FJsonObject>& Specification,
	FUeremcpAbilityVariationPlan& OutPlan,
	FString& OutError)
{
	OutPlan = FUeremcpAbilityVariationPlan();
	OutError.Reset();
	if (!Specification.IsValid())
	{
		OutError = TEXT("create_spell_variation requires a specification object");
		return false;
	}
	static const TSet<FString> AllowedFields = {
		TEXT("source_binding"),
		TEXT("target_row"),
		TEXT("presentation_asset"),
		TEXT("verification_mode"),
	};
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Specification->Values)
	{
		if (!AllowedFields.Contains(Pair.Key))
		{
			OutError = FString::Printf(TEXT("specification contains unknown field '%s'"), *FString(Pair.Key));
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* Binding = nullptr;
	if (!Specification->TryGetObjectField(TEXT("source_binding"), Binding)
		|| !Binding || !Binding->IsValid())
	{
		OutError = TEXT("specification.source_binding is required");
		return false;
	}
	static const TSet<FString> BindingFields = {
		TEXT("ability_table"), TEXT("source_row"), TEXT("vfx_phase"),
	};
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Binding)->Values)
	{
		if (!BindingFields.Contains(Pair.Key))
		{
			OutError = FString::Printf(
				TEXT("specification.source_binding contains unknown field '%s'"),
				*FString(Pair.Key));
			return false;
		}
	}
	if (!(*Binding)->TryGetStringField(TEXT("ability_table"), OutPlan.SourceTablePath)
		|| !(*Binding)->TryGetStringField(TEXT("source_row"), OutPlan.SourceRow)
		|| !(*Binding)->TryGetStringField(TEXT("vfx_phase"), OutPlan.VfxPhase)
		|| !Specification->TryGetStringField(TEXT("target_row"), OutPlan.TargetRow)
		|| !Specification->TryGetStringField(TEXT("presentation_asset"), OutPlan.PresentationAsset))
	{
		OutError = TEXT("ability_table, source_row, vfx_phase, target_row, and presentation_asset are required strings");
		return false;
	}
	FString VerificationMode;
	if (!Specification->TryGetStringField(TEXT("verification_mode"), VerificationMode)
		|| VerificationMode != TEXT("protected_fields_equal"))
	{
		OutError = TEXT("verification_mode must be protected_fields_equal");
		return false;
	}
	if (!FPackageName::IsValidLongPackageName(OutPlan.SourceTablePath)
		|| !IsStableRowName(OutPlan.SourceRow)
		|| !IsStableRowName(OutPlan.TargetRow))
	{
		OutError = TEXT("ability_table or row identity is invalid");
		return false;
	}
	if (OutPlan.VfxPhase != TEXT("projectile")
		&& OutPlan.VfxPhase != TEXT("impact")
		&& OutPlan.VfxPhase != TEXT("projectile_and_impact"))
	{
		OutError = TEXT("vfx_phase must be projectile, impact, or projectile_and_impact");
		return false;
	}

	TSharedPtr<FJsonObject> SourceRow;
	if (!ReadRow(OutPlan.SourceTablePath, OutPlan.SourceRow, SourceRow, OutError))
	{
		return false;
	}
	OutPlan.ProtectedFieldNames = {
		TEXT("CastType"),
		TEXT("Speed"),
		TEXT("Range"),
		TEXT("ProjRadius"),
		TEXT("GravityScale"),
		TEXT("Homing"),
		TEXT("ImpactDamage"),
		TEXT("ImpactStatus"),
		TEXT("StatusDuration"),
		TEXT("AoeRadius"),
		TEXT("EscalateTo"),
		TEXT("SpawnEntity"),
		TEXT("EntityLengthCm"),
		TEXT("EntityThicknessCm"),
		TEXT("EntityHeightCm"),
	};
	OutPlan.SourceProtectedFields =
		SelectProtectedFields(SourceRow, OutPlan.ProtectedFieldNames, OutError);
	if (!OutPlan.SourceProtectedFields.IsValid())
	{
		return false;
	}

	SourceRow->SetStringField(TEXT("AbilityId"), OutPlan.TargetRow);
	if (OutPlan.VfxPhase == TEXT("projectile")
		|| OutPlan.VfxPhase == TEXT("projectile_and_impact"))
	{
		SourceRow->SetStringField(TEXT("ProjectileNS"), OutPlan.PresentationAsset);
	}
	if (OutPlan.VfxPhase == TEXT("impact")
		|| OutPlan.VfxPhase == TEXT("projectile_and_impact"))
	{
		SourceRow->SetStringField(TEXT("ImpactNS"), OutPlan.PresentationAsset);
	}

	OutPlan.SpellPlan.RowName = OutPlan.TargetRow;
	OutPlan.SpellPlan.RowPayload = SourceRow;
	OutPlan.SpellPlan.DependencyAssetPaths = { OutPlan.PresentationAsset };
	OutPlan.SpellPlan.StaticChecks = {
		TEXT("composite_source_binding_resolved"),
		TEXT("source_freabilitydef_normalized"),
		TEXT("pattern_b_owner_unchanged"),
		TEXT("presentation_only_override_planned"),
	};
	return true;
}

bool FUeremcpAbilityVariation::VerifyTarget(
	const FString& TargetTablePath,
	const FUeremcpAbilityVariationPlan& Plan,
	TSharedPtr<FJsonObject>& OutTargetProtectedFields,
	FString& OutError)
{
	TSharedPtr<FJsonObject> TargetRow;
	if (!ReadRow(TargetTablePath, Plan.TargetRow, TargetRow, OutError))
	{
		return false;
	}
	OutTargetProtectedFields =
		SelectProtectedFields(TargetRow, Plan.ProtectedFieldNames, OutError);
	if (!OutTargetProtectedFields.IsValid())
	{
		return false;
	}
	FString SourceHashError;
	FString TargetHashError;
	const FString SourceHash =
		FUeremcpContentHash::HashJsonObject(Plan.SourceProtectedFields, &SourceHashError);
	const FString TargetHash =
		FUeremcpContentHash::HashJsonObject(OutTargetProtectedFields, &TargetHashError);
	if (SourceHash.IsEmpty() || TargetHash.IsEmpty() || SourceHash != TargetHash)
	{
		OutError = FString::Printf(
			TEXT("protected gameplay fields drifted (source=%s target=%s)"),
			*SourceHash,
			*TargetHash);
		return false;
	}
	return true;
}
