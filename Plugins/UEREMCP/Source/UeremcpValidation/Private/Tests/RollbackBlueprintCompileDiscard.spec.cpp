// RB-06 ADR-0005 open q4: Blueprint compile + Discard disk/AR/CDO hazards.
#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationRollbackBp
{
	using namespace UE::ToolsetRegistry;
}

namespace UeremcpValidationTests
{
	static UBlueprint* CreateSaveCompileScratchBlueprint(
		const FString& Suite,
		const FString& AssetName,
		FAutomationTestBase& Test)
	{
		const FString PackagePath = UeremcpMakeScratchPackagePath(Suite, AssetName);

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("CreatePackage"), Package))
		{
			return nullptr;
		}
		Package->FullyLoad();

		UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
		if (!Test.TestNotNull(TEXT("CreateBlueprint"), BP))
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);

		const FString Filename = PackageToFilesystemPath(PackagePath);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FSavePackageResultStruct Result = UPackage::Save(Package, BP, *Filename, SaveArgs);
		if (!Test.TestTrue(TEXT("UPackage::Save Blueprint succeeded"), Result.Result == ESavePackageResult::Success))
		{
			return nullptr;
		}
		return BP;
	}

	static bool BlueprintHasMemberVariable(UBlueprint* BP, const FName VarName)
	{
		if (!BP)
		{
			return false;
		}
		for (const FBPVariableDescription& Var : BP->NewVariables)
		{
			if (Var.VarName == VarName)
			{
				return true;
			}
		}
		return false;
	}
}

/**
 * ADR-0005 open q4 probe.
 *
 * Create trivial Actor BP → save+compile → Enter → add member var → compile+save
 * → Discard+Leave → assert on-disk package matches pre-sandbox bytes; record CDO hazard.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpRollbackBlueprintCompileDiscard,
	"UEREMCP.Validation.Rollback.BlueprintCompileDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpRollbackBlueprintCompileDiscard::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationTests;
	using namespace UeremcpValidationRollbackBp;

	static const FString SuiteName = TEXT("Rollback_BlueprintCompileDiscard");
	static const FString SandboxName = TEXT("UEREMCP_Rollback_BlueprintCompileDiscard");
	static const FString AssetName = TEXT("TrivialActorBP");
	static const FName SandboxVarName(TEXT("UeremcpSandboxOnlyVar"));

	FUeremcpScratchGuard Guard(SuiteName);
	EnsureSandboxInactive(*this);

	UBlueprint* BP = CreateSaveCompileScratchBlueprint(SuiteName, AssetName, *this);
	if (!BP)
	{
		return false;
	}

	const FString PackagePath = UeremcpMakeScratchPackagePath(SuiteName, AssetName);
	const FString SoftPath = PackagePath + TEXT(".") + AssetName;
	const FString FsPath = PackageToFilesystemPath(PackagePath);

	TArray<uint8> PreSandboxBytes;
	if (!ReadFileBytes(FsPath, PreSandboxBytes, *this))
	{
		return false;
	}

	TestFalse(TEXT("no sandbox var before Enter"), BlueprintHasMemberVariable(BP, SandboxVarName));

	if (!TestTrue(TEXT("FGlobalSandbox::Enter"), FGlobalSandbox::Enter(SandboxName, TEXT("BlueprintCompileDiscard probe"))))
	{
		AddError(TEXT("BLOCKER: Enter failed — cannot evaluate q4."));
		return false;
	}

	FEdGraphPinType FloatPinType;
	FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
	FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

	TestTrue(
		TEXT("AddMemberVariable inside sandbox"),
		FBlueprintEditorUtils::AddMemberVariable(BP, SandboxVarName, FloatPinType, TEXT("1.0")));

	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
	BP->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	const FSavePackageResultStruct SaveResult = UPackage::Save(BP->GetPackage(), BP, *FsPath, SaveArgs);
	if (!TestTrue(TEXT("save inside sandbox"), SaveResult.Result == ESavePackageResult::Success))
	{
		FGlobalSandbox::Discard();
		FGlobalSandbox::Leave();
		return false;
	}

	TestTrue(TEXT("sandbox var present before Discard"), BlueprintHasMemberVariable(BP, SandboxVarName));

	if (!DiscardAndLeaveSandbox(*this))
	{
		return false;
	}

	TArray<uint8> PostDiscardBytes;
	if (!ReadFileBytes(FsPath, PostDiscardBytes, *this))
	{
		return false;
	}

	const bool bDiskRestored = PreSandboxBytes == PostDiscardBytes;
	if (!bDiskRestored)
	{
		AddError(TEXT("Q4 NEGATIVE: Discard did not restore pre-sandbox Blueprint package bytes on disk."));
	}
	else
	{
		AddInfo(TEXT("Q4 POSITIVE (disk): pre-sandbox .uasset bytes match after Discard+Leave."));
	}

	if (!AssertAssetFullyPresent(PackagePath, SoftPath, *this))
	{
		return false;
	}

	// Reload from disk to inspect authoritative state vs in-memory CDO hazard.
	UBlueprint* ReloadedBP = LoadObject<UBlueprint>(nullptr, *SoftPath);
	const bool bReloadedHasVar = BlueprintHasMemberVariable(ReloadedBP, SandboxVarName);
	if (bReloadedHasVar)
	{
		AddError(TEXT("Q4 NEGATIVE: reloaded Blueprint still has sandbox-only member variable after Discard."));
	}
	else
	{
		AddInfo(TEXT("Q4 POSITIVE (reload): sandbox-only member variable absent after reload from disk."));
	}

	// CDO / bytecode hazard probe — in-memory object may differ from disk.
	bool bCdoHazard = false;
	if (UBlueprintGeneratedClass* GenClass = Cast<UBlueprintGeneratedClass>(BP->GeneratedClass))
	{
		if (UObject* CDO = GenClass->GetDefaultObject(false))
		{
			AddInfo(FString::Printf(
				TEXT("CDO probe: pre-reload BP still loaded; CDO=%s (may retain stale bytecode until purge/reload)"),
				*CDO->GetPathName()));
			if (BlueprintHasMemberVariable(BP, SandboxVarName))
			{
				bCdoHazard = true;
				AddWarning(TEXT("Q4 HAZARD: in-memory Blueprint still lists sandbox variable after Discard — "
					"C DO/bytecode may be stale until explicit reload. See ADR-0005 open q4."));
			}
		}
	}

	const bool bPass = bDiskRestored && !bReloadedHasVar;
	if (!bPass)
	{
		return false;
	}

	if (bCdoHazard)
	{
		AddInfo(TEXT("Q4 MITIGATED: disk/AR restored; in-memory CDO hazard documented — not a full q4 PASS for bytecode."));
	}
	else
	{
		AddInfo(TEXT("Q4 POSITIVE (this run): disk restored and reload clean; no in-memory CDO hazard observed."));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
