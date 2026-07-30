// Agent-facing execute_plan action adapter tests (ADR-0008 / ADR-0009 / ADR-0006).
//
// Uses deterministic fake handlers — domain handlers are integration-owned.
// AICallable registration lives on UUeremcpReferenceToolset (WS-03); see
// docs/proposals/ws-05-execute-plan-aicallable.md.

#include "UeremcpPlanActions.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpEnvelope.h"
#include "UeremcpIdempotency.h"
#include "UeremcpJobActions.h"
#include "UeremcpJobRegistry.h"
#include "UeremcpPlanExecutor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpPlanActionsTest
{
	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	void Reset()
	{
		FUeremcpPlanExecutor::ClearActionHandlers();
		FUeremcpPlanExecutor::ClearTransactionCallbacks();
		FUeremcpPlanActions::ClearForceTimeoutForTests();
		FUeremcpJobRegistry::Get().Clear();
		FUeremcpIdempotencyStore::Get().SetDurableEnabled(false);
		FUeremcpIdempotencyStore::Get().Clear();
	}

	bool RegisterOkAction(FString& OutError)
	{
		return FUeremcpPlanExecutor::RegisterAction(
			TEXT("fake_create"),
			[](const FString&, FString& OutResponseJson, FString& Error)
			{
				OutResponseJson = TEXT(R"JSON(
					{"protocol_version":"1.0","status":"created_and_validated",
					"summary":"fake created","metrics":{"mcp_round_trips":1,"internal_operations":2},
					"result":{"asset_path":"/Game/__UeremcpTests/Fake"},
					"revision":"rev-fake-1"}
				)JSON");
				Error.Reset();
				return true;
			},
			OutError);
	}

	bool RegisterFailAction(FString& OutError)
	{
		return FUeremcpPlanExecutor::RegisterAction(
			TEXT("fake_fail"),
			[](const FString&, FString& OutResponseJson, FString& Error)
			{
				OutResponseJson = TEXT(R"JSON(
					{"protocol_version":"1.0","status":"failed_validation",
					"summary":"fake failed","metrics":{"mcp_round_trips":1,"internal_operations":1}}
				)JSON");
				Error.Reset();
				return true;
			},
			OutError);
	}

	bool RegisterAtomicTxn(bool& bRolledBack, FString& OutError)
	{
		FUeremcpPlanTransactionCallbacks Transaction;
		Transaction.Begin = [](FString&) { return true; };
		Transaction.Commit = [](FString&) { return true; };
		Transaction.Rollback = [&bRolledBack](FString&)
		{
			bRolledBack = true;
			return true;
		};
		return FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanActionsParseDispatchTest,
	"UEREMCP.Protocol.PlanActions.ParseDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanActionsParseDispatchTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanActionsTest;
	Reset();

	const TSharedPtr<FJsonObject> WrongAction = Parse(FUeremcpPlanActions::ExecutePlan(
		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"bad-action\","
			"\"action\":\"ping\",\"specification\":{\"operations\":[{"
			"\"id\":\"a\",\"action\":\"fake_create\"}]}}")));
	TestEqual(
		TEXT("wrong action rejects"),
		WrongAction->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));

	const TSharedPtr<FJsonObject> MissingSpec = Parse(FUeremcpPlanActions::ExecutePlan(
		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"bad-spec\","
			"\"action\":\"execute_plan\"}")));
	TestEqual(
		TEXT("missing specification rejects"),
		MissingSpec->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));

	FString Error;
	bool bRolledBack = false;
	TestTrue(TEXT("fake handler registers"), RegisterOkAction(Error));
	TestTrue(TEXT("txn registers"), RegisterAtomicTxn(bRolledBack, Error));

	const TSharedPtr<FJsonObject> MissingHandler = Parse(FUeremcpPlanActions::ExecutePlan(TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"missing-handler","action":"execute_plan",
		"options":{"timeout_ms":0},
		"specification":{"transaction":{"atomic":true},
		"operations":[{"id":"x","action":"not_registered"}],
		"on_failure":"rollback_all"}}
	)JSON")));
	TestEqual(
		TEXT("missing handler rejects before mutation"),
		MissingHandler->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));
	TestTrue(
		TEXT("missing handler error names the action"),
		MissingHandler->GetStringField(TEXT("summary")).Contains(TEXT("not_registered")));
	TestFalse(TEXT("reject path does not roll back"), bRolledBack);

	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanActionsSuccessTest,
	"UEREMCP.Protocol.PlanActions.Success",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanActionsSuccessTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanActionsTest;
	Reset();
	FString Error;
	bool bRolledBack = false;
	TestTrue(TEXT("fake handler registers"), RegisterOkAction(Error));
	TestTrue(TEXT("txn registers"), RegisterAtomicTxn(bRolledBack, Error));

	const FString ResponseJson = FUeremcpPlanActions::ExecutePlan(TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-ok","action":"execute_plan",
		"idempotency_key":"plan-actions-ok-1",
		"options":{"timeout_ms":0,"dry_run":true},
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true},
		"operations":[{"id":"create","action":"fake_create"}],
		"on_failure":"rollback_all"}}
	)JSON"));
	const TSharedPtr<FJsonObject> Response = Parse(ResponseJson);
	TestEqual(
		TEXT("success status is validated"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("created_and_validated")));
	TestEqual(
		TEXT("understood action follows ADR-0003 envelope"),
		Response->GetObjectField(TEXT("understood"))->GetStringField(TEXT("action")),
		FString(TEXT("execute_plan")));
	TestEqual(
		TEXT("success keeps request id"),
		Response->GetStringField(TEXT("request_id")),
		FString(TEXT("plan-ok")));
	TestEqual(
		TEXT("success uses current protocol version"),
		Response->GetStringField(TEXT("protocol_version")),
		FUeremcpEnvelope::ProtocolVersion());
	TestTrue(TEXT("success has actionable summary"), !Response->GetStringField(TEXT("summary")).IsEmpty());
	TestEqual(
		TEXT("success is one MCP round trip"),
		static_cast<int32>(
			Response->GetObjectField(TEXT("metrics"))->GetNumberField(TEXT("mcp_round_trips"))),
		1);
	TestFalse(TEXT("success does not roll back"), bRolledBack);

	const FString ReplayJson = FUeremcpPlanActions::ExecutePlan(TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-ok-replay","action":"execute_plan",
		"idempotency_key":"plan-actions-ok-1",
		"options":{"timeout_ms":0,"dry_run":true},
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true},
		"operations":[{"id":"create","action":"fake_create"}],
		"on_failure":"rollback_all"}}
	)JSON"));
	const TSharedPtr<FJsonObject> Replay = Parse(ReplayJson);
	TestEqual(
		TEXT("replay keeps prior status"),
		Replay->GetStringField(TEXT("status")),
		FString(TEXT("created_and_validated")));
	TestEqual(
		TEXT("replay uses current request id"),
		Replay->GetStringField(TEXT("request_id")),
		FString(TEXT("plan-ok-replay")));
	TestEqual(
		TEXT("replay keeps understood action"),
		Replay->GetObjectField(TEXT("understood"))->GetStringField(TEXT("action")),
		FString(TEXT("execute_plan")));
	TestTrue(
		TEXT("replay annotated"),
		Replay->GetObjectField(TEXT("metrics"))->GetBoolField(TEXT("replayed")));

	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanActionsRollbackTest,
	"UEREMCP.Protocol.PlanActions.HandlerFailureRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanActionsRollbackTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanActionsTest;
	Reset();
	FString Error;
	bool bRolledBack = false;
	TestTrue(TEXT("fail handler registers"), RegisterFailAction(Error));
	TestTrue(TEXT("txn registers"), RegisterAtomicTxn(bRolledBack, Error));

	const TSharedPtr<FJsonObject> Response = Parse(FUeremcpPlanActions::ExecutePlan(TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-fail","action":"execute_plan",
		"options":{"timeout_ms":0},
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true},
		"operations":[{"id":"boom","action":"fake_fail"}],
		"on_failure":"rollback_all"}}
	)JSON")));
	TestEqual(
		TEXT("handler failure reports rolled_back"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("rolled_back")));
	TestTrue(TEXT("rollback callback ran"), bRolledBack);
	const TSharedPtr<FJsonObject>* Rollback = nullptr;
	TestTrue(
		TEXT("rollback evidence present"),
		Response->TryGetObjectField(TEXT("rollback"), Rollback)
			&& Rollback
			&& (*Rollback)->GetBoolField(TEXT("performed")));

	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanActionsTimeoutPartialTest,
	"UEREMCP.Protocol.PlanActions.TimeoutPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanActionsTimeoutPartialTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanActionsTest;
	Reset();
	FString Error;
	bool bRolledBack = false;
	TestTrue(TEXT("fake handler registers"), RegisterOkAction(Error));
	TestTrue(TEXT("txn registers"), RegisterAtomicTxn(bRolledBack, Error));

	FUeremcpPlanActions::SetForceTimeoutForTests([]() { return true; });

	const TSharedPtr<FJsonObject> Partial = Parse(FUeremcpPlanActions::ExecutePlan(TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-timeout","action":"execute_plan",
		"options":{"timeout_ms":5000},
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true},
		"operations":[{"id":"create","action":"fake_create"}],
		"on_failure":"rollback_all"}}
	)JSON")));
	TestEqual(
		TEXT("forced timeout is partially_completed"),
		Partial->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestTrue(TEXT("timeout returns job handle"), Partial->HasField(TEXT("job")));
	TestEqual(
		TEXT("timeout job is still running"),
		Partial->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
		FString(TEXT("running")));
	TestEqual(
		TEXT("timeout is one round trip"),
		static_cast<int32>(
			Partial->GetObjectField(TEXT("metrics"))->GetNumberField(TEXT("mcp_round_trips"))),
		1);
	TestFalse(TEXT("forced timeout does not execute handlers"), bRolledBack);

	const FString JobId = Partial->GetObjectField(TEXT("job"))->GetStringField(TEXT("job_id"));
	const TSharedPtr<FJsonObject> Poll = Parse(FUeremcpJobActions::GetJobResult(
		FString::Printf(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"poll-plan\","
				"\"action\":\"get_job_result\",\"specification\":{\"job_id\":\"%s\"}}"),
			*JobId)));
	TestEqual(
		TEXT("poll keeps running until domain completes"),
		Poll->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
		FString(TEXT("running")));

	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanActionsTimeoutCompleteTest,
	"UEREMCP.Protocol.PlanActions.TimeoutCompletesInline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanActionsTimeoutCompleteTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanActionsTest;
	Reset();
	FString Error;
	bool bRolledBack = false;
	TestTrue(TEXT("fake handler registers"), RegisterOkAction(Error));
	TestTrue(TEXT("txn registers"), RegisterAtomicTxn(bRolledBack, Error));

	const TSharedPtr<FJsonObject> Response = Parse(FUeremcpPlanActions::ExecutePlan(TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-fast","action":"execute_plan",
		"options":{"timeout_ms":120000},
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true},
		"operations":[{"id":"create","action":"fake_create"}],
		"on_failure":"rollback_all"}}
	)JSON")));
	TestEqual(
		TEXT("fast positive-timeout path validates"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("created_and_validated")));
	TestEqual(
		TEXT("fast path keeps understood action"),
		Response->GetObjectField(TEXT("understood"))->GetStringField(TEXT("action")),
		FString(TEXT("execute_plan")));
	TestFalse(TEXT("fast path does not roll back"), bRolledBack);

	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanActionsValidationOnlyTest,
	"UEREMCP.Protocol.PlanActions.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanActionsValidationOnlyTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanActionsTest;
	Reset();

	const TSharedPtr<FJsonObject> WrongAction = Parse(FUeremcpPlanActions::ExecutePlan(
		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"v1\","
			"\"action\":\"get_job_result\",\"specification\":{\"job_id\":\"x\"}}")));
	TestEqual(
		TEXT("non-plan action rejects"),
		WrongAction->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));

	const TSharedPtr<FJsonObject> MissingOps = Parse(FUeremcpPlanActions::ExecutePlan(
		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"v2\","
			"\"action\":\"execute_plan\",\"specification\":{}}")));
	TestEqual(
		TEXT("empty specification rejects at executor"),
		MissingOps->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));

	Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
