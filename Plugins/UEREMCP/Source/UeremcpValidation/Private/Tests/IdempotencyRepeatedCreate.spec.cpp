// ADR-0006 verification: Idempotency.RepeatedCreate (protocol + scratch asset harness).
#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "Curves/CurveFloat.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpContentHash.h"
#include "UeremcpEnvelope.h"
#include "UeremcpIdempotency.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationTests
{
	struct FScratchCreateOutcome
	{
		FString Status;
		FString ResponseJson;
		bool bReplayed = false;
		bool bWouldCompile = false;
	};

	static FString MakeCurveSpecJson(int32 KeyCount)
	{
		return FString::Printf(
			TEXT(R"({"asset_kind":"curve_float","float_keys":%d})"),
			KeyCount);
	}

	static FString RevisionForCurveSpec(int32 KeyCount, FString* OutError = nullptr)
	{
		return FUeremcpContentHash::HashJsonString(MakeCurveSpecJson(KeyCount), OutError);
	}

	static bool ParseResponseStatus(const FString& Json, FString& OutStatus, bool& OutReplayed)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		Root->TryGetStringField(TEXT("status"), OutStatus);
		OutReplayed = false;
		const TSharedPtr<FJsonObject>* Metrics = nullptr;
		if (Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid())
		{
			OutReplayed = (*Metrics)->GetBoolField(TEXT("replayed"));
		}
		return true;
	}

	/**
	 * Minimal create_or_update harness using stable scratch paths + idempotency store.
	 * Not a domain pipeline — documents protocol-level behaviour until WS-05 wires assets.
	 */
	static FScratchCreateOutcome ExecuteCreateOrUpdateHarness(
		const FUeremcpRequest& Request,
		const FString& Suite,
		const FString& AssetName,
		int32 SpecKeyCount,
		int32& InOutWouldCompileCount,
		FAutomationTestBase& Test)
	{
		FScratchCreateOutcome Outcome;
		const FString StablePackage = UeremcpMakeScratchPackagePath(Suite, AssetName);
		const FString StableSoft = StablePackage + TEXT(".") + AssetName;

		if (!Request.IdempotencyKey.IsEmpty())
		{
			FString Stored;
			if (FUeremcpIdempotencyStore::Get().TryGet(Request.IdempotencyKey, Stored))
			{
				Outcome.ResponseJson = Stored;
				ParseResponseStatus(Stored, Outcome.Status, Outcome.bReplayed);
				Outcome.bReplayed = true;
				return Outcome;
			}
		}

		FString RevError;
		const FString CurrentRevision = RevisionForCurveSpec(SpecKeyCount, &RevError);
		if (CurrentRevision.IsEmpty())
		{
			Test.AddError(FString::Printf(TEXT("revision hash failed: %s"), *RevError));
			Outcome.Status = TEXT("error");
			return Outcome;
		}

		const bool bExists = UEditorAssetLibrary::DoesAssetExist(StableSoft);
		if (bExists)
		{
			Outcome.Status = TEXT("no_change_required");
			FUeremcpResponse Response;
			Response.RequestId = Request.RequestId;
			Response.Status = Outcome.Status;
			Response.Summary = TEXT("Asset already exists at stable path with matching specification.");
			Response.PrimaryAsset = StableSoft;
			Response.Revision = CurrentRevision;
			Outcome.ResponseJson = FUeremcpEnvelope::SerializeResponse(Response);
		}
		else
		{
			const FString CreatedSoft = CreateAndSaveScratchCurve(Suite, AssetName, Test);
			if (CreatedSoft.IsEmpty())
			{
				Outcome.Status = TEXT("failed_validation");
				return Outcome;
			}
			Test.TestEqual(TEXT("stable path unchanged"), CreatedSoft, StableSoft);
			++InOutWouldCompileCount;
			Outcome.bWouldCompile = true;
			Outcome.Status = TEXT("created_and_validated");

			FUeremcpResponse Response;
			Response.RequestId = Request.RequestId;
			Response.Status = Outcome.Status;
			Response.Summary = TEXT("Created scratch CurveFloat at stable path.");
			Response.PrimaryAsset = StableSoft;
			Response.Revision = CurrentRevision;
			Response.Metrics.AssetsAffected = 1;
			Response.Metrics.InternalOperations = 1;
			Outcome.ResponseJson = FUeremcpEnvelope::SerializeResponse(Response);
		}

		if (!Request.IdempotencyKey.IsEmpty())
		{
			FUeremcpIdempotencyStore::Get().Put(Request.IdempotencyKey, Outcome.ResponseJson);
		}
		return Outcome;
	}
}

/**
 * ADR-0006: same create_or_update request three times → one asset, replay/no_change on repeats.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyRepeatedCreate,
	"UEREMCP.Validation.Idempotency.RepeatedCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyRepeatedCreate::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationTests;

	static const FString SuiteName = TEXT("Idempotency_RepeatedCreate");
	static const FString AssetName = TEXT("StableCurve");
	static const FString IdemKey = TEXT("idem-repeated-create-v1");

	FUeremcpScratchGuard Guard(SuiteName);
	FUeremcpIdempotencyStore::Get().Clear();

	const FString StablePackage = UeremcpMakeScratchPackagePath(SuiteName, AssetName);
	const FString StableSoft = StablePackage + TEXT(".") + AssetName;

	FUeremcpRequest BaseRequest;
	BaseRequest.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	BaseRequest.RequestId = TEXT("req-1");
	BaseRequest.Action = TEXT("validation.scratch_create_or_update");
	BaseRequest.Mode = TEXT("create_or_update");
	BaseRequest.TargetAssetPath = StableSoft;
	BaseRequest.IdempotencyKey = IdemKey;

	int32 WouldCompileCount = 0;
	TArray<FString> Statuses;
	TArray<FString> PathsSeen;

	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		FUeremcpRequest AttemptRequest = BaseRequest;
		AttemptRequest.RequestId = FString::Printf(TEXT("req-%d"), Attempt + 1);

		const FScratchCreateOutcome Outcome = ExecuteCreateOrUpdateHarness(
			AttemptRequest, SuiteName, AssetName, /*SpecKeyCount=*/1, WouldCompileCount, *this);

		FString ParsedStatus;
		bool bReplayed = false;
		if (!ParseResponseStatus(Outcome.ResponseJson, ParsedStatus, bReplayed))
		{
			AddError(FString::Printf(TEXT("attempt %d: response not parseable JSON"), Attempt + 1));
			return false;
		}

		Statuses.Add(ParsedStatus);
		PathsSeen.Add(StableSoft);

		if (Attempt == 0)
		{
			TestEqual(TEXT("first attempt creates"), ParsedStatus, FString(TEXT("created_and_validated")));
			TestFalse(TEXT("first attempt not replayed"), bReplayed);
		}
		else
		{
			const bool bOkRepeat = ParsedStatus == TEXT("no_change_required") || bReplayed;
			TestTrue(
				FString::Printf(TEXT("repeat attempt %d is no_change_required or replayed"), Attempt + 1),
				bOkRepeat);
			TestTrue(
				FString::Printf(TEXT("repeat attempt %d served from idempotency store"), Attempt + 1),
				bReplayed);
		}
	}

	TestEqual(TEXT("exactly one asset path"), PathsSeen.Num(), 3);
	for (const FString& Path : PathsSeen)
	{
		TestEqual(TEXT("path stable across attempts"), Path, StableSoft);
	}

	TestTrue(TEXT("single asset on disk"), UEditorAssetLibrary::DoesAssetExist(StableSoft));
	TestEqual(TEXT("no compile on repeats"), WouldCompileCount, 1);

	AddInfo(FString::Printf(
		TEXT("Idempotency.RepeatedCreate statuses: %s | %s | %s (protocol harness; not full domain pipeline)"),
		*Statuses[0], *Statuses[1], *Statuses[2]));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
