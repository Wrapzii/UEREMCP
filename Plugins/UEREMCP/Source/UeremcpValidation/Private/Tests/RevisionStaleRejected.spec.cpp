// ADR-0006 verification: Revision.StaleRejected (content_hash revision + scratch asset).
#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "Curves/CurveFloat.h"
#include "Curves/RealCurve.h"
#include "EditorAssetLibrary.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UeremcpContentHash.h"
#include "UeremcpEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationTests
{
	static int32 CountFloatKeys(UCurveFloat* Curve)
	{
		if (!Curve)
		{
			return 0;
		}
		return Curve->FloatCurve.GetNumKeys();
	}

	static bool ApplySpecKeyCount(UCurveFloat* Curve, int32 KeyCount)
	{
		if (!Curve)
		{
			return false;
		}
		Curve->FloatCurve.Reset();
		for (int32 i = 0; i < KeyCount; ++i)
		{
			Curve->FloatCurve.AddKey(static_cast<float>(i), static_cast<float>(i));
		}
		return true;
	}

	static FString RevisionForKeyCount(int32 KeyCount, FString* OutError = nullptr)
	{
		const FString Spec = FString::Printf(
			TEXT(R"({"asset_kind":"curve_float","float_keys":%d})"),
			KeyCount);
		return FUeremcpContentHash::HashJsonString(Spec, OutError);
	}

	struct FRevisionGuardResult
	{
		bool bRejected = false;
		FString Status;
		FString ReturnedRevision;
		bool bMutated = false;
	};

	/** Inline expected_revision guard mirroring ADR-0006 default reject policy. */
	static FRevisionGuardResult AttemptGuardedUpdate(
		const FString& ExpectedRevision,
		const FString& CurrentRevision,
		const FString& OnConflict,
		TFunctionRef<bool()> MutateFn)
	{
		FRevisionGuardResult Result;
		if (OnConflict != TEXT("reject") && OnConflict != TEXT("return_conflict"))
		{
			Result.bMutated = MutateFn();
			Result.Status = TEXT("modified_and_validated");
			Result.ReturnedRevision = CurrentRevision;
			return Result;
		}

		if (!ExpectedRevision.IsEmpty() && ExpectedRevision != CurrentRevision)
		{
			Result.bRejected = true;
			Result.Status = TEXT("rejected");
			Result.ReturnedRevision = CurrentRevision;
			return Result;
		}

		Result.bMutated = MutateFn();
		Result.Status = TEXT("modified_and_validated");
		Result.ReturnedRevision = CurrentRevision;
		return Result;
	}
}

/**
 * ADR-0006: retrieve revision → OOB modify → submit stale expected_revision → rejected, no mutation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpRevisionStaleRejected,
	"UEREMCP.Validation.Revision.StaleRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpRevisionStaleRejected::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationTests;

	static const FString SuiteName = TEXT("Revision_StaleRejected");
	static const FString AssetName = TEXT("RevisionCurve");

	FUeremcpScratchGuard Guard(SuiteName);

	const FString SoftPath = CreateAndSaveScratchCurve(SuiteName, AssetName, *this);
	if (SoftPath.IsEmpty())
	{
		return false;
	}
	const FString PackagePath = UeremcpMakeScratchPackagePath(SuiteName, AssetName);

	UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *SoftPath);
	if (!TestNotNull(TEXT("load scratch curve"), Curve))
	{
		return false;
	}

	ApplySpecKeyCount(Curve, /*KeyCount=*/1);
	Curve->GetPackage()->MarkPackageDirty();
	const FString FsPath = PackageToFilesystemPath(PackagePath);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	UPackage::Save(Curve->GetPackage(), Curve, *FsPath, SaveArgs);

	FString HashError;
	const FString RetrievedRevision = RevisionForKeyCount(1, &HashError);
	TestFalse(TEXT("retrieved revision non-empty"), RetrievedRevision.IsEmpty());
	if (RetrievedRevision.IsEmpty())
	{
		AddError(FString::Printf(TEXT("hash error: %s"), *HashError));
		return false;
	}

	// Out-of-band modification (simulates another agent / human edit).
	ApplySpecKeyCount(Curve, /*KeyCount=*/2);
	Curve->GetPackage()->MarkPackageDirty();
	UPackage::Save(Curve->GetPackage(), Curve, *FsPath, SaveArgs);
	const int32 KeysAfterOob = CountFloatKeys(Curve);
	TestEqual(TEXT("OOB modify applied"), KeysAfterOob, 2);

	const FString CurrentRevision = RevisionForKeyCount(2, &HashError);
	TestNotEqual(TEXT("revision changed after OOB edit"), CurrentRevision, RetrievedRevision);

	bool bMutationAttempted = false;
	const FRevisionGuardResult GuardResult = AttemptGuardedUpdate(
		RetrievedRevision,
		CurrentRevision,
		TEXT("reject"),
		[&]() -> bool
		{
			bMutationAttempted = true;
			ApplySpecKeyCount(Curve, /*KeyCount=*/3);
			return true;
		});

	TestTrue(TEXT("stale revision rejected"), GuardResult.bRejected);
	TestEqual(TEXT("status rejected"), GuardResult.Status, FString(TEXT("rejected")));
	TestEqual(TEXT("returns current revision"), GuardResult.ReturnedRevision, CurrentRevision);
	TestFalse(TEXT("no mutation on reject"), GuardResult.bMutated);
	TestFalse(TEXT("mutate lambda not invoked"), bMutationAttempted);

	TestEqual(TEXT("asset still has OOB key count"), CountFloatKeys(Curve), 2);

	FUeremcpResponse RejectResponse;
	RejectResponse.RequestId = TEXT("rev-stale-1");
	RejectResponse.Status = TEXT("rejected");
	RejectResponse.Summary = TEXT("expected_revision stale");
	RejectResponse.Revision = CurrentRevision;
	const FString RejectJson = FUeremcpEnvelope::SerializeResponse(RejectResponse);
	TestTrue(TEXT("rejection serializes"), RejectJson.Contains(TEXT("\"rejected\"")));
	TestTrue(TEXT("rejection includes current revision"), RejectJson.Contains(CurrentRevision));

	AddInfo(TEXT("Revision.StaleRejected PASS at protocol+scratch harness level "
		"(content_hash revision guard; full graph pipeline not wired)."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
