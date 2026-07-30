// Shared helpers for UEREMCP.Validation automation tests (WS-11 / RB-06).
#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

class UCurveFloat;

namespace UeremcpValidationTests
{
	/** Ensure no sandbox is active; Discard+Leave if one is. */
	void EnsureSandboxInactive(FAutomationTestBase& Test);

	/** Convert long package name to absolute filesystem .uasset path. */
	FString PackageToFilesystemPath(const FString& LongPackageName);

	/** True when the real (non-sandbox) Content file exists. */
	bool RealDiskFileExists(const FString& LongPackageName);

	/** Create + save a trivial UCurveFloat under /Game/__UeremcpTests/<Suite>/<Name>. */
	FString CreateAndSaveScratchCurve(const FString& Suite, const FString& AssetName, FAutomationTestBase& Test);

	/** Asset registry lookup for a package name. */
	bool AssetRegistryHasPackage(const FString& LongPackageName);

	/** Discard active sandbox and leave. Returns false if either step fails. */
	bool DiscardAndLeaveSandbox(FAutomationTestBase& Test);

	/** Assert disk / EditorAssetLibrary / AR / FindPackage clean for discarded adds. */
	bool AssertPackagesFullyGone(
		const TArray<FString>& PackagePaths,
		const TArray<FString>& SoftPaths,
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel);

	/** Assert asset exists on disk, in AR, and via EditorAssetLibrary. */
	bool AssertAssetFullyPresent(const FString& PackagePath, const FString& SoftPath, FAutomationTestBase& Test);

	/** Read entire file bytes for hash/compare. */
	bool ReadFileBytes(const FString& Path, TArray<uint8>& OutBytes, FAutomationTestBase& Test);
}
