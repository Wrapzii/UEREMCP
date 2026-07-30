// WS-11 editor regression for WS-06 commit 204a0d3.
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpEnvelope.h"

#if __has_include("UeremcpBlueprintMutatingGate.h")
#include "UeremcpBlueprintMutatingGate.h"
#define UEREMCP_HAS_BLUEPRINT_MUTATING_GATE 1
#else
#define UEREMCP_HAS_BLUEPRINT_MUTATING_GATE 0
#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationBlueprintDispatch
{
	static FString Request(const FString& RequestId)
	{
		FString ProjectPath = FPaths::GetProjectFilePath();
		ProjectPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return FString::Printf(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"%s\","
				"\"action\":\"submit_graph\",\"mode\":\"replace\","
				"\"project\":{\"path\":\"%s\"},"
				"\"target\":{\"asset_path\":\"/Game/__UeremcpTests/BP_DispatchProbe\"}}"),
			*RequestId,
			*ProjectPath);
	}

	static TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintMutatingDispatchGateRegression,
	"UEREMCP.Validation.Blueprint.MutatingDispatchGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintMutatingDispatchGateRegression::RunTest(const FString& Parameters)
{
#if !UEREMCP_HAS_BLUEPRINT_MUTATING_GATE
	AddWarning(TEXT(
		"UEREMCP_BLUEPRINT_DISPATCH_OUTCOME=SKIP reason=ws06_204a0d3_not_merged"));
	return true;
#else
	using namespace UeremcpValidationBlueprintDispatch;

	FUeremcpBlueprintMutatingGate First;
	FString FirstBlock;
	if (!TestTrue(
			TEXT("first Blueprint adapter acquires mutator"),
			First.TryBeginMutating(Request(TEXT("ws11-bp-dispatch-a")), true, FirstBlock)))
	{
		AddInfo(TEXT("UEREMCP_BLUEPRINT_DISPATCH_OUTCOME=FAIL reason=first_not_admitted"));
		return false;
	}
	TestTrue(TEXT("first adapter active"), First.IsActive());
	TestTrue(TEXT("first adapter has no blocking response"), FirstBlock.IsEmpty());

	FUeremcpBlueprintMutatingGate Second;
	FString SecondBlock;
	TestFalse(
		TEXT("second Blueprint adapter is queued"),
		Second.TryBeginMutating(Request(TEXT("ws11-bp-dispatch-b")), true, SecondBlock));
	const TSharedPtr<FJsonObject> Queued = Parse(SecondBlock);
	if (!TestTrue(TEXT("queued response parseable"), Queued.IsValid()))
	{
		FUeremcpResponse Cleanup;
		Cleanup.RequestId = TEXT("ws11-bp-dispatch-a");
		Cleanup.Status = TEXT("failed_validation");
		Cleanup.Summary = TEXT("Dispatch regression cleanup.");
		First.Complete(Cleanup);
		AddInfo(TEXT("UEREMCP_BLUEPRINT_DISPATCH_OUTCOME=FAIL reason=queue_response_invalid"));
		return false;
	}
	FString QueuedStatus;
	Queued->TryGetStringField(TEXT("status"), QueuedStatus);
	TestEqual(TEXT("contender reports partial"), QueuedStatus, FString(TEXT("partially_completed")));
	TestTrue(TEXT("contender exposes job"), Queued->HasTypedField<EJson::Object>(TEXT("job")));

	FUeremcpResponse CompleteResponse;
	CompleteResponse.RequestId = TEXT("ws11-bp-dispatch-a");
	CompleteResponse.Status = TEXT("no_change_required");
	CompleteResponse.Summary = TEXT("Dispatch regression released first slot.");
	First.Complete(CompleteResponse);
	TestFalse(TEXT("first adapter inactive after Complete"), First.IsActive());

	FUeremcpBlueprintMutatingGate Promoted;
	FString PromotedBlock;
	TestTrue(
		TEXT("queued request acquires after release"),
		Promoted.TryBeginMutating(Request(TEXT("ws11-bp-dispatch-b")), true, PromotedBlock));
	TestTrue(TEXT("promoted request not blocked"), PromotedBlock.IsEmpty());
	CompleteResponse.RequestId = TEXT("ws11-bp-dispatch-b");
	Promoted.Complete(CompleteResponse);

	const bool bPass = !HasAnyErrors();
	AddInfo(bPass
		? TEXT("UEREMCP_BLUEPRINT_DISPATCH_OUTCOME=PASS proof=adapter_queue_release")
		: TEXT("UEREMCP_BLUEPRINT_DISPATCH_OUTCOME=FAIL reason=assertion_failure"));
	AddInfo(TEXT("A6 is not claimed: this regression proves dispatch admission/serialization only."));
	return bPass;
#endif
}

#endif // WITH_DEV_AUTOMATION_TESTS
