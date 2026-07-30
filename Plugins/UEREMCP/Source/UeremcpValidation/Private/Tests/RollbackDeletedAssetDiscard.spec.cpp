// RB-06 ADR-0005 open q5: does Discard() restore assets deleted inside the sandbox?
#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "EditorAssetLibrary.h"
#include "Misc/AutomationTest.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "Types/SandboxedFileChangeInfo.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace UE::ToolsetRegistry;
using namespace UE::FileSandboxCore;

/**
 * ADR-0005 open q5 probe.
 *
 * Sequence:
 *   Create+save asset OUTSIDE sandbox → Enter → DeleteAsset → GetChanges (Removed?)
 *   → Discard+Leave → assert disk/AR/EditorAssetLibrary restored.
 *
 * Honest: NEGATIVE is a valid deliverable and must be documented.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpRollbackDeletedAssetDiscard,
	"UEREMCP.Validation.Rollback.DeletedAssetDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpRollbackDeletedAssetDiscard::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationTests;

	static const FString SuiteName = TEXT("Rollback_DeletedAssetDiscard");
	static const FString SandboxName = TEXT("UEREMCP_Rollback_DeletedAssetDiscard");
	static const FString AssetName = TEXT("PreExistingCurve");

	FUeremcpScratchGuard Guard(SuiteName);
	EnsureSandboxInactive(*this);
	TestFalse(TEXT("no sandbox active at start"), FGlobalSandbox::IsActive());

	const FString SoftPath = CreateAndSaveScratchCurve(SuiteName, AssetName, *this);
	if (SoftPath.IsEmpty())
	{
		return false;
	}
	const FString PackagePath = UeremcpMakeScratchPackagePath(SuiteName, AssetName);
	const FString FsPath = PackageToFilesystemPath(PackagePath);

	if (!AssertAssetFullyPresent(PackagePath, SoftPath, *this))
	{
		return false;
	}

	TArray<uint8> PreDeleteBytes;
	if (!ReadFileBytes(FsPath, PreDeleteBytes, *this))
	{
		return false;
	}

	if (!TestTrue(TEXT("FGlobalSandbox::Enter"), FGlobalSandbox::Enter(SandboxName, TEXT("DeletedAssetDiscard probe"))))
	{
		AddError(TEXT("BLOCKER: Enter failed — cannot evaluate q5."));
		return false;
	}

	TestTrue(TEXT("asset visible inside sandbox before delete"), UEditorAssetLibrary::DoesAssetExist(SoftPath));

	if (!TestTrue(TEXT("DeleteAsset inside sandbox"), UEditorAssetLibrary::DeleteAsset(SoftPath)))
	{
		FGlobalSandbox::Discard();
		FGlobalSandbox::Leave();
		return false;
	}

	TestFalse(TEXT("asset gone inside sandbox after delete"), UEditorAssetLibrary::DoesAssetExist(SoftPath));

	bool bRemovalObserved = false;
	const TArray<FSandboxedFileChangeInfo> Changes = FGlobalSandbox::GetChanges();
	AddInfo(FString::Printf(TEXT("GetChanges after delete: %d entries"), Changes.Num()));
	for (const FSandboxedFileChangeInfo& Info : Changes)
	{
		AddInfo(FString::Printf(TEXT("  change path=%s action=%d"), *Info.Path, static_cast<int32>(Info.Action)));
		if (Info.Action == ESandboxFileChange::Removed
			|| Info.Path.Contains(TEXT("PreExistingCurve"))
			|| FPaths::IsSamePath(Info.Path, FsPath))
		{
			bRemovalObserved = true;
		}
	}
	if (!bRemovalObserved)
	{
		AddWarning(TEXT("Q5: deletion did not produce an obvious Removed entry in GetChanges — may still revert on Discard."));
	}

	if (!DiscardAndLeaveSandbox(*this))
	{
		return false;
	}

	const bool bRestored = AssertAssetFullyPresent(PackagePath, SoftPath, *this);
	TArray<uint8> PostDiscardBytes;
	const bool bReadPost = ReadFileBytes(FsPath, PostDiscardBytes, *this);
	const bool bBytesMatch = bReadPost && PreDeleteBytes == PostDiscardBytes;

	if (!bRestored || !bBytesMatch)
	{
		AddError(TEXT("Q5 NEGATIVE: Discard did NOT fully restore a pre-existing deleted asset on disk/AR. "
			"ADR-0005 atomic rollback must NOT claim deletion coverage until this passes or is mitigated."));
		return false;
	}

	AddInfo(TEXT("Q5 POSITIVE (this run): Discard+Leave restored pre-existing asset deleted inside sandbox."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
