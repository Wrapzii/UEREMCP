// FileSandbox empirical probes + Rollback.MultiAssetDiscard (RB-06 / ADR-0005).
//
// Until this test PASSES at runtime, rollback.available must report false
// (ADR-0005 Verification). Do not claim atomic multi-asset rollback from headers alone.
//
#include "UeremcpScratchPaths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "Types/SandboxFileChange.h"
#include "Types/SandboxedFileChangeInfo.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationTests
{
	using namespace UE::ToolsetRegistry;
	using namespace UE::FileSandboxCore;

	static const FString SuiteName = TEXT("Rollback_MultiAssetDiscard");
	static const FString SandboxName = TEXT("UEREMCP_Rollback_MultiAssetDiscard");

	static FString PackageToFilesystemPath(const FString& LongPackageName)
	{
		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(LongPackageName, Filename, FPackageName::GetAssetPackageExtension()))
		{
			return FString();
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}

	static bool RealDiskFileExists(const FString& LongPackageName)
	{
		const FString Path = PackageToFilesystemPath(LongPackageName);
		return !Path.IsEmpty() && IFileManager::Get().FileExists(*Path);
	}

	/**
	 * Create + save a trivial concrete asset under the scratch suite.
	 * Uses UCurveFloat (concrete) — UDataAsset is abstract and cannot be NewObject'd.
	 * Returns soft object path.
	 */
	static FString CreateAndSaveScratchAsset(const FString& Suite, const FString& AssetName, FAutomationTestBase& Test)
	{
		const FString PackagePath = UeremcpMakeScratchPackagePath(Suite, AssetName);
		const FString AssetSoftPath = PackagePath + TEXT(".") + AssetName;

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("CreatePackage"), Package))
		{
			return FString();
		}
		Package->FullyLoad();

		UCurveFloat* Asset = NewObject<UCurveFloat>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!Test.TestNotNull(TEXT("NewObject<UCurveFloat>"), Asset))
		{
			return FString();
		}

		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();

		const FString Filename = PackageToFilesystemPath(PackagePath);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *Filename, SaveArgs);
		if (!Test.TestTrue(TEXT("UPackage::Save succeeded"), Result.Result == ESavePackageResult::Success))
		{
			return FString();
		}
		return AssetSoftPath;
	}

	static bool AssetRegistryHasPackage(const FString& LongPackageName)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByPackageName(FName(*LongPackageName), Assets, /*bIncludeOnlyOnDiskAssets=*/true);
		if (Assets.Num() > 0)
		{
			return true;
		}
		AR.GetAssetsByPackageName(FName(*LongPackageName), Assets, /*bIncludeOnlyOnDiskAssets=*/false);
		return Assets.Num() > 0;
	}
}

/**
 * Smoke test: proves the UeremcpValidation module loaded and scratch helpers work.
 * This is the Wave 1 "one passing editor integration test" gate (RB-14).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpValidationHarnessSmoke,
	"UEREMCP.Validation.Harness.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpValidationHarnessSmoke::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationTests;

	const FString SmokeSuite = TEXT("Harness_Smoke");
	FUeremcpScratchGuard Guard(SmokeSuite);
	const FString Soft = CreateAndSaveScratchAsset(SmokeSuite, TEXT("SmokeAsset"), *this);
	if (Soft.IsEmpty())
	{
		return false;
	}

	const FString PackagePath = UeremcpMakeScratchPackagePath(SmokeSuite, TEXT("SmokeAsset"));
	TestTrue(TEXT("asset exists via EditorAssetLibrary"), UEditorAssetLibrary::DoesAssetExist(Soft));
	TestTrue(TEXT("package file exists on disk (via IFileManager through active platform file)"), RealDiskFileExists(PackagePath));

	// Explicit cleanup before guard fires so we can assert it worked.
	const int32 Deleted = UeremcpCleanupScratchSuite(SmokeSuite);
	TestTrue(TEXT("cleanup deleted at least one asset"), Deleted >= 1);
	TestFalse(TEXT("asset gone after cleanup"), UEditorAssetLibrary::DoesAssetExist(Soft));
	return true;
}

/**
 * RB-06 q1 + q3 probe + ADR-0005 Rollback.MultiAssetDiscard gate.
 *
 * Sequence:
 *   Enter sandbox → create/save N CurveFloat assets under /Game/__UeremcpTests/ → GetChanges
 *   → assert package saves appear in GetChanges (q1) → deliberate "failure" → Discard
 *   → Leave → assert real Content/ disk clean, asset registry clean, no live UPackage
 *     for discarded adds (q3) → retry create outside sandbox succeeds.
 *
 * Honest statuses:
 *   - If Enter/GetChanges/Discard APIs fail → test fails, rollback.available stays false.
 *   - If package saves are NOT in GetChanges → q1 NEGATIVE: ADR-0005 has a hole.
 *   - If Discard leaves AR/UObject stale → q3 NEGATIVE: not a usable rollback.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpRollbackMultiAssetDiscard,
	"UEREMCP.Validation.Rollback.MultiAssetDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpRollbackMultiAssetDiscard::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationTests;

	FUeremcpScratchGuard Guard(SuiteName);

	// Precondition: no leftover sandbox.
	if (FGlobalSandbox::IsActive())
	{
		AddWarning(FString::Printf(TEXT("Sandbox already active (%s); Discard+Leave before test"), *FGlobalSandbox::GetActiveName()));
		FGlobalSandbox::Discard();
		FGlobalSandbox::Leave();
	}

	TestFalse(TEXT("no sandbox active at start"), FGlobalSandbox::IsActive());

	if (!TestTrue(TEXT("FGlobalSandbox::Enter"), FGlobalSandbox::Enter(SandboxName, TEXT("UEREMCP Rollback.MultiAssetDiscard"))))
	{
		AddError(TEXT("BLOCKER: FGlobalSandbox::Enter failed — cannot evaluate q1/q3. FileSandbox plugin may be disabled."));
		return false;
	}

	TArray<FString> SoftPaths;
	TArray<FString> PackagePaths;
	constexpr int32 AssetCount = 3;
	for (int32 i = 0; i < AssetCount; ++i)
	{
		const FString Name = FString::Printf(TEXT("DiscardAsset_%d"), i);
		const FString Soft = CreateAndSaveScratchAsset(SuiteName, Name, *this);
		if (Soft.IsEmpty())
		{
			FGlobalSandbox::Discard();
			FGlobalSandbox::Leave();
			return false;
		}
		SoftPaths.Add(Soft);
		PackagePaths.Add(UeremcpMakeScratchPackagePath(SuiteName, Name));
	}

	// --- q1: do package saves appear in GetChanges? ---
	const TArray<FSandboxedFileChangeInfo> Changes = FGlobalSandbox::GetChanges();
	AddInfo(FString::Printf(TEXT("GetChanges returned %d entries after %d UPackage::Save calls"), Changes.Num(), AssetCount));
	for (const FSandboxedFileChangeInfo& Info : Changes)
	{
		AddInfo(FString::Printf(TEXT("  change path=%s action=%d"), *Info.Path, static_cast<int32>(Info.Action)));
	}

	bool bAnyPackageChangeObserved = false;
	for (const FString& PackagePath : PackagePaths)
	{
		const FString ExpectedFs = PackageToFilesystemPath(PackagePath);
		for (const FSandboxedFileChangeInfo& Info : Changes)
		{
			if (FPaths::IsSamePath(Info.Path, ExpectedFs)
				|| Info.Path.Contains(TEXT("__UeremcpTests"))
				|| Info.Path.Contains(TEXT("DiscardAsset_")))
			{
				bAnyPackageChangeObserved = true;
				break;
			}
		}
	}

	if (!bAnyPackageChangeObserved)
	{
		// NEGATIVE FINDING — do not claim interception.
		AddError(TEXT("Q1 NEGATIVE: UPackage::Save of scratch assets did not produce matching GetChanges entries. "
			"FileSandbox may not intercept package saves for this path, or saves landed outside tracked mount points. "
			"ADR-0005 atomic rollback MUST NOT be claimed. See docs/proposals/ws-11-adr-0005-sandbox-semantics.md"));
		FGlobalSandbox::Discard();
		FGlobalSandbox::Leave();
		UeremcpCleanupScratchSuite(SuiteName);
		return false;
	}

	AddInfo(TEXT("Q1 POSITIVE (this run): package saves observed in FGlobalSandbox::GetChanges after UPackage::Save."));

	// While sandboxed, the active IPlatformFile may report the file as existing even
	// if the real Content/ tree does not yet hold it. Record both views.
	for (const FString& PackagePath : PackagePaths)
	{
		const FString Fs = PackageToFilesystemPath(PackagePath);
		AddInfo(FString::Printf(
			TEXT("pre-Discard: IFileManager sees '%s' exists=%d"),
			*Fs, IFileManager::Get().FileExists(*Fs) ? 1 : 0));
	}

	// Deliberate failure point — discard everything.
	if (!TestTrue(TEXT("FGlobalSandbox::Discard"), FGlobalSandbox::Discard()))
	{
		AddError(TEXT("Discard failed"));
		FGlobalSandbox::Leave();
		return false;
	}

	// Discard leaves the sandbox active [VERIFIED: SandboxLibrary.h Discard docs].
	TestTrue(TEXT("sandbox still active after Discard"), FGlobalSandbox::IsActive());
	TestEqual(TEXT("GetChanges empty after Discard"), FGlobalSandbox::GetChanges().Num(), 0);

	if (!TestTrue(TEXT("FGlobalSandbox::Leave"), FGlobalSandbox::Leave()))
	{
		AddError(TEXT("Leave failed after Discard"));
		return false;
	}
	TestFalse(TEXT("sandbox inactive after Leave"), FGlobalSandbox::IsActive());

	// --- q3: asset registry + in-memory UObject after Discard ---
	bool bQ3Clean = true;
	for (int32 i = 0; i < PackagePaths.Num(); ++i)
	{
		const FString& PackagePath = PackagePaths[i];
		const FString& Soft = SoftPaths[i];
		const FString Fs = PackageToFilesystemPath(PackagePath);

		// After Leave, IFileManager should see the real Content tree.
		const bool bDisk = !Fs.IsEmpty() && IFileManager::Get().FileExists(*Fs);
		if (bDisk)
		{
			AddError(FString::Printf(TEXT("Q3 NEGATIVE: real disk file still exists after Discard+Leave: %s"), *Fs));
			bQ3Clean = false;
		}

		if (UEditorAssetLibrary::DoesAssetExist(Soft))
		{
			AddError(FString::Printf(TEXT("Q3 NEGATIVE: EditorAssetLibrary still reports asset after Discard+Leave: %s"), *Soft));
			bQ3Clean = false;
		}

		if (AssetRegistryHasPackage(PackagePath))
		{
			AddError(FString::Printf(TEXT("Q3 NEGATIVE: AssetRegistry still has package after Discard+Leave: %s"), *PackagePath));
			bQ3Clean = false;
		}

		if (FindPackage(nullptr, *PackagePath) != nullptr)
		{
			// RevertAll is documented to PurgePackages for added files
			// [VERIFIED: SandboxInstance.cpp RevertAll → PackageReloadHandler->PurgePackages].
			// If FindPackage still returns non-null, purge did not fully clear memory.
			AddError(FString::Printf(TEXT("Q3 NEGATIVE: FindPackage still returns in-memory UPackage after Discard+Leave: %s"), *PackagePath));
			bQ3Clean = false;
		}
	}

	if (!bQ3Clean)
	{
		AddError(TEXT("Q3 NEGATIVE overall: Discard left stale disk/AR/UObject state. Not a usable atomic rollback."));
		UeremcpCleanupScratchSuite(SuiteName);
		return false;
	}

	AddInfo(TEXT("Q3 POSITIVE (this run): after Discard+Leave, disk/AR/FindPackage clean for discarded adds."));

	// Retry: identical creates outside sandbox must succeed (idempotent recovery).
	TArray<FString> RetrySoft;
	for (int32 i = 0; i < AssetCount; ++i)
	{
		const FString Name = FString::Printf(TEXT("DiscardAsset_%d"), i);
		const FString Soft = CreateAndSaveScratchAsset(SuiteName, Name, *this);
		if (Soft.IsEmpty())
		{
			AddError(TEXT("Retry create after Discard failed — editor not fully usable"));
			UeremcpCleanupScratchSuite(SuiteName);
			return false;
		}
		RetrySoft.Add(Soft);
	}
	for (const FString& Soft : RetrySoft)
	{
		TestTrue(TEXT("retry asset exists"), UEditorAssetLibrary::DoesAssetExist(Soft));
	}

	AddInfo(TEXT("Rollback.MultiAssetDiscard PASSED for CurveFloat adds. "
		"Does NOT prove Blueprint compile/CDO discard, deletions, or Config/Saved coverage."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
