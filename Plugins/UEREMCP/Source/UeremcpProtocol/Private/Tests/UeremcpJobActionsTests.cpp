// Core-facing ADR-0009 action adapter tests.

#include "UeremcpJobActions.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpEnvelope.h"
#include "UeremcpJobRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpJobActionsTest
{
	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	FString Request(
		const FString& RequestId,
		const FString& Action,
		const FString& JobId)
	{
		return FString::Printf(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"%s\","
				"\"action\":\"%s\",\"specification\":{\"job_id\":\"%s\"}}"),
			*RequestId,
			*Action,
			*JobId);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpGetJobResultActionTest,
	"UEREMCP.Protocol.JobActions.GetJobResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpGetJobResultActionTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpJobActionsTest;
	FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();
	Registry.Clear();
	FString Error;
	FString JobId;
	TestTrue(
		TEXT("shared-registry job created"),
		Registry.CreateJob(TEXT("origin-request"), false, TEXT("Working"), JobId, Error));
	TestTrue(TEXT("shared-registry job starts"), Registry.StartJob(JobId, Error));

	const TSharedPtr<FJsonObject> Running = Parse(FUeremcpJobActions::GetJobResult(
		Request(TEXT("poll-request"), TEXT("get_job_result"), JobId)));
	TestEqual(
		TEXT("poll response uses current request id"),
		Running->GetStringField(TEXT("request_id")),
		FString(TEXT("poll-request")));
	TestEqual(
		TEXT("running poll is partial"),
		Running->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestEqual(
		TEXT("running poll preserves job id"),
		Running->GetObjectField(TEXT("job"))->GetStringField(TEXT("job_id")),
		JobId);
	TestEqual(
		TEXT("action poll increments cumulative round trips"),
		static_cast<int32>(
			Running->GetObjectField(TEXT("metrics"))
				->GetNumberField(TEXT("mcp_round_trips"))),
		2);

	FUeremcpResponse Terminal;
	Terminal.RequestId = TEXT("origin-request");
	Terminal.Status = TEXT("created_and_validated");
	Terminal.Summary = TEXT("Verified terminal result.");
	Terminal.Metrics.McpRoundTrips = 1;
	Terminal.Metrics.InternalOperations = 3;
	TestTrue(TEXT("job completes"), Registry.CompleteJob(JobId, Terminal, Error));

	const TSharedPtr<FJsonObject> Completed = Parse(FUeremcpJobActions::GetJobResult(
		Request(TEXT("terminal-poll"), TEXT("get_job_result"), JobId)));
	TestEqual(
		TEXT("terminal action returns retained status"),
		Completed->GetStringField(TEXT("status")),
		FString(TEXT("created_and_validated")));
	TestEqual(
		TEXT("terminal action exposes completed state"),
		Completed->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
		FString(TEXT("completed")));
	TestEqual(
		TEXT("terminal action counts both polls"),
		static_cast<int32>(
			Completed->GetObjectField(TEXT("metrics"))
				->GetNumberField(TEXT("mcp_round_trips"))),
		3);

	const TSharedPtr<FJsonObject> Missing = Parse(FUeremcpJobActions::GetJobResult(
		Request(TEXT("missing-poll"), TEXT("get_job_result"), TEXT("missing"))));
	TestEqual(
		TEXT("missing id rejects"),
		Missing->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));
	Registry.Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCancelJobActionTest,
	"UEREMCP.Protocol.JobActions.CancelJob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpCancelJobActionTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpJobActionsTest;
	FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();
	Registry.Clear();
	FString Error;
	FString JobId;
	int32 CancelCheckpoints = 0;
	TestTrue(
		TEXT("cancellable shared-registry job created"),
		Registry.CreateJob(
			TEXT("cancel-origin"),
			true,
			TEXT("Running"),
			JobId,
			Error,
			[&CancelCheckpoints]()
			{
				++CancelCheckpoints;
				return true;
			}));
	TestTrue(TEXT("cancellable job starts"), Registry.StartJob(JobId, Error));

	const TSharedPtr<FJsonObject> Cancelled = Parse(FUeremcpJobActions::CancelJob(
		Request(TEXT("cancel-request"), TEXT("cancel_job"), JobId)));
	TestEqual(TEXT("cancel callback runs once"), CancelCheckpoints, 1);
	TestEqual(
		TEXT("cancel action uses current request id"),
		Cancelled->GetStringField(TEXT("request_id")),
		FString(TEXT("cancel-request")));
	TestEqual(
		TEXT("cancel action stays honest"),
		Cancelled->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestEqual(
		TEXT("cancel action reaches cancelled state"),
		Cancelled->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
		FString(TEXT("cancelled")));

	FString NonCancellableId;
	TestTrue(
		TEXT("non-cancellable job created"),
		Registry.CreateJob(
			TEXT("no-cancel-origin"),
			false,
			FString(),
			NonCancellableId,
			Error));
	const TSharedPtr<FJsonObject> Rejected = Parse(FUeremcpJobActions::CancelJob(
		Request(TEXT("no-cancel-request"), TEXT("cancel_job"), NonCancellableId)));
	TestEqual(
		TEXT("non-cancellable action rejects"),
		Rejected->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));
	Registry.Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobActionValidationTest,
	"UEREMCP.Protocol.JobActions.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobActionValidationTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpJobActionsTest;
	const TSharedPtr<FJsonObject> WrongAction = Parse(FUeremcpJobActions::GetJobResult(
		Request(TEXT("wrong"), TEXT("cancel_job"), TEXT("job"))));
	TestEqual(
		TEXT("wrong action rejects"),
		WrongAction->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));

	const TSharedPtr<FJsonObject> MissingJob = Parse(FUeremcpJobActions::CancelJob(
		TEXT("{\"protocol_version\":\"1.0\",\"action\":\"cancel_job\","
			"\"specification\":{}}")));
	TestEqual(
		TEXT("missing job id rejects"),
		MissingJob->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
