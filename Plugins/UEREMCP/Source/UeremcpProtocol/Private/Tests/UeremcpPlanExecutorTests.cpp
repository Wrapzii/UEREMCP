// ADR-0008 execute_plan interpreter tests.

#include "UeremcpPlanExecutor.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpPlanExecutorTest
{
	void Reset()
	{
		FUeremcpPlanExecutor::ClearActionHandlers();
		FUeremcpPlanExecutor::ClearTransactionCallbacks();
	}

	FString MakeSuccess(
		const FString& Status,
		const FString& Summary,
		const FString& PrimaryAsset = FString())
	{
		FUeremcpResponse Response;
		Response.Status = Status;
		Response.Summary = Summary;
		Response.PrimaryAsset = PrimaryAsset;
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = 1;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorCompositionTest,
	"UEREMCP.Protocol.PlanExecutor.Composition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorCompositionTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanExecutorTest;
	Reset();
	FString Error;
	bool bBegin = false;
	bool bCommit = false;
	bool bRollback = false;
	bool bReferenceResolved = false;
	bool bInternalTimeoutInline = false;

	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [&bBegin](FString&) { bBegin = true; return true; };
	Transaction.Commit = [&bCommit](FString&) { bCommit = true; return true; };
	Transaction.Rollback = [&bRollback](FString&) { bRollback = true; return true; };
	TestTrue(
		TEXT("transaction callbacks register"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));

	TestTrue(
		TEXT("material handler registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_material"),
			[](const FString&, FString& OutResponse, FString&)
			{
				OutResponse = MakeSuccess(
					TEXT("created_and_validated"),
					TEXT("material created"),
					TEXT("/Game/M_Test"));
				return true;
			},
			Error));
	TestTrue(
		TEXT("effect handler registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_effect"),
			[&bReferenceResolved, &bInternalTimeoutInline](
				const FString& RequestJson,
				FString& OutResponse,
				FString&)
			{
				const TSharedPtr<FJsonObject> Request = Parse(RequestJson);
				const TSharedPtr<FJsonObject> Specification =
					Request->GetObjectField(TEXT("specification"));
				FString Material;
				bReferenceResolved =
					Specification->TryGetStringField(TEXT("material"), Material)
					&& Material == TEXT("/Game/M_Test");
				bInternalTimeoutInline =
					Request->GetObjectField(TEXT("options"))
						->GetNumberField(TEXT("timeout_ms")) == 0.0;
				OutResponse = MakeSuccess(
					TEXT("modified_and_validated"),
					TEXT("effect created"));
				return bReferenceResolved && bInternalTimeoutInline;
			},
			Error));

	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"plan-1",
		"action":"execute_plan",
		"specification":{
			"operations":[
				{"id":"material","action":"create_material"},
				{
					"id":"effect",
					"action":"create_effect",
					"depends_on":["material"],
					"specification":{"material":{"$ref":"material.result.primary_asset"}}
				}
			]
		}
	})");
	FString ResponseJson;
	TestTrue(
		TEXT("composed plan succeeds"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestTrue(TEXT("transaction began"), bBegin);
	TestTrue(TEXT("transaction committed"), bCommit);
	TestFalse(TEXT("transaction did not roll back"), bRollback);
	TestTrue(TEXT("dependent reference resolved"), bReferenceResolved);
	TestTrue(TEXT("nested operations forced inline"), bInternalTimeoutInline);

	const TSharedPtr<FJsonObject> Response = Parse(ResponseJson);
	TestEqual(
		TEXT("aggregate status reflects terminal operation"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("modified_and_validated")));
	const TArray<TSharedPtr<FJsonValue>>& Operations =
		Response->GetObjectField(TEXT("result"))->GetArrayField(TEXT("operations"));
	TestEqual(TEXT("two operation results returned"), Operations.Num(), 2);
	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorRollbackBasicTest,
	"UEREMCP.Protocol.PlanExecutor.RollbackBasic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorRollbackBasicTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanExecutorTest;
	Reset();
	FString Error;
	bool bRollback = false;
	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [](FString&) { return true; };
	Transaction.Commit = [](FString&) { return true; };
	Transaction.Rollback = [&bRollback](FString&) { bRollback = true; return true; };
	TestTrue(
		TEXT("transaction callbacks register"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));
	TestTrue(
		TEXT("failing handler registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("fail_operation"),
			[](const FString&, FString&, FString& OutError)
			{
				OutError = TEXT("expected failure");
				return false;
			},
			Error));

	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"plan-rollback",
		"action":"execute_plan",
		"specification":{"operations":[{"id":"fail","action":"fail_operation"}]}
	})");
	FString ResponseJson;
	TestTrue(
		TEXT("required failure returns structured response"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestTrue(TEXT("required failure rolls back"), bRollback);
	TestEqual(
		TEXT("rollback status is honest"),
		Parse(ResponseJson)->GetStringField(TEXT("status")),
		FString(TEXT("rolled_back")));
	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorFailClosedTest,
	"UEREMCP.Protocol.PlanExecutor.FailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorFailClosedTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanExecutorTest;
	Reset();
	FString Error;
	bool bBegin = false;
	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [&bBegin](FString&) { bBegin = true; return true; };
	Transaction.Commit = [](FString&) { return true; };
	Transaction.Rollback = [](FString&) { return true; };
	TestTrue(
		TEXT("transaction callbacks register"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));

	const FString MissingHandler = TEXT(R"({
		"protocol_version":"1.0",
		"action":"execute_plan",
		"specification":{"operations":[{"id":"missing","action":"missing_action"}]}
	})");
	FString ResponseJson;
	TestTrue(
		TEXT("missing handler returns structured rejection"),
		FUeremcpPlanExecutor::ExecuteRequest(MissingHandler, ResponseJson, Error));
	TestFalse(TEXT("missing handler rejects before begin"), bBegin);
	TestEqual(
		TEXT("missing handler status"),
		Parse(ResponseJson)->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));

	TestTrue(
		TEXT("condition handler registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("condition_action"),
			[](const FString&, FString& OutResponse, FString&)
			{
				OutResponse = MakeSuccess(TEXT("no_change_required"), TEXT("unused"));
				return true;
			},
			Error));
	const FString UnsupportedCondition = TEXT(R"({
		"protocol_version":"1.0",
		"action":"execute_plan",
		"specification":{"operations":[{
			"id":"condition",
			"action":"condition_action",
			"condition":{"asset_exists":"/Game/A"}
		}]}
	})");
	TestTrue(
		TEXT("unsupported asset condition returns structured rejection"),
		FUeremcpPlanExecutor::ExecuteRequest(UnsupportedCondition, ResponseJson, Error));
	TestFalse(TEXT("unsupported condition rejects before begin"), bBegin);
	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorNonAtomicTest,
	"UEREMCP.Protocol.PlanExecutor.NonAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorNonAtomicTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpPlanExecutorTest;
	Reset();
	FString Error;
	TestTrue(
		TEXT("non-atomic handler registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("inspect_only"),
			[](const FString&, FString& OutResponse, FString&)
			{
				OutResponse = MakeSuccess(TEXT("no_change_required"), TEXT("inspected"));
				return true;
			},
			Error));
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":false,"rollback_on_failure":false},
			"operations":[{"id":"inspect","action":"inspect_only"}],
			"on_failure":"stop"
		}
	})");
	FString ResponseJson;
	TestTrue(
		TEXT("non-atomic plan does not require transaction callbacks"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestEqual(
		TEXT("non-atomic status"),
		Parse(ResponseJson)->GetStringField(TEXT("status")),
		FString(TEXT("no_change_required")));
	Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
// ADR-0008 execute_plan parser, dispatch, reference, and rollback tests.

#include "UeremcpPlanExecutor.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	void ResetExecutor()
	{
		FUeremcpPlanExecutor::ClearActionHandlers();
		FUeremcpPlanExecutor::ClearTransactionCallbacks();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorPreflightTest,
	"UEREMCP.Protocol.PlanExecutor.Preflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorPreflightTest::RunTest(const FString& Parameters)
{
	ResetExecutor();
	const FString Request = TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-preflight","action":"execute_plan",
		"specification":{"transaction":{"atomic":false,"rollback_on_failure":false},
		"operations":[{"id":"missing","action":"missing_action"}],"on_failure":"stop"}}
	)JSON");

	FString ResponseJson;
	FString Error;
	TestTrue(
		TEXT("preflight produces a structured response"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	const TSharedPtr<FJsonObject> Response = ParseJson(ResponseJson);
	TestTrue(TEXT("preflight response parses"), Response.IsValid());
	TestEqual(
		TEXT("missing handler rejects before mutation"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("rejected")));
	TestTrue(
		TEXT("missing action is named"),
		Response->GetStringField(TEXT("summary")).Contains(TEXT("missing_action")));
	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorDispatchTest,
	"UEREMCP.Protocol.PlanExecutor.DispatchAndRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorDispatchTest::RunTest(const FString& Parameters)
{
	ResetExecutor();
	FString Error;
	TArray<FString> DispatchOrder;

	TestTrue(
		TEXT("producer registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("produce_asset"),
			[&DispatchOrder](
				const FString& RequestJson,
				FString& OutResponseJson,
				FString& OutError)
			{
				DispatchOrder.Add(TEXT("producer"));
				OutResponseJson = TEXT(R"JSON(
					{"protocol_version":"1.0","status":"created_and_validated","summary":"producer ok",
					"result":{"primary_asset":"/Game/Test/A","created_assets":[{"asset_path":"/Game/Test/A"}]},
					"validation":{"compiled":true,"checks_performed":["producer.checked"]},
					"changes":[{"asset_path":"/Game/Test/A","kind":"created"}],
					"revision":"sha256:producer","metrics":{"mcp_round_trips":1,"internal_operations":3}}
				)JSON");
				OutError.Reset();
				return true;
			},
			Error));
	TestTrue(
		TEXT("consumer registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("consume_asset"),
			[this, &DispatchOrder](
				const FString& RequestJson,
				FString& OutResponseJson,
				FString& OutError)
			{
				DispatchOrder.Add(TEXT("consumer"));
				const TSharedPtr<FJsonObject> Request = ParseJson(RequestJson);
				const TSharedPtr<FJsonObject>* Specification = nullptr;
				FString InputAsset;
				const bool bResolved = Request.IsValid()
					&& Request->TryGetObjectField(TEXT("specification"), Specification)
					&& Specification
					&& (*Specification)->TryGetStringField(TEXT("input_asset"), InputAsset)
					&& InputAsset == TEXT("/Game/Test/A");
				TestTrue(TEXT("consumer receives resolved producer ref"), bResolved);
				OutResponseJson = TEXT(R"JSON(
					{"protocol_version":"1.0","status":"modified_and_validated","summary":"consumer ok",
					"result":{"primary_asset":"/Game/Test/B","modified_assets":[{"asset_path":"/Game/Test/B"}]},
					"validation":{"saved":true,"checks_performed":["consumer.checked"]},
					"changes":[{"asset_path":"/Game/Test/B","kind":"modified"}],
					"revision":"sha256:consumer","metrics":{"mcp_round_trips":1,"internal_operations":5}}
				)JSON");
				OutError.Reset();
				return true;
			},
			Error));

	bool bBegan = false;
	bool bCommitted = false;
	bool bRolledBack = false;
	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [&bBegan](FString&)
	{
		bBegan = true;
		return true;
	};
	Transaction.Commit = [&bCommitted](FString&)
	{
		bCommitted = true;
		return true;
	};
	Transaction.Rollback = [&bRolledBack](FString&)
	{
		bRolledBack = true;
		return true;
	};
	TestTrue(
		TEXT("transaction coordinator registers"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));

	const FString Request = TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-dispatch","action":"execute_plan",
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true,"compile_policy":"at_boundaries","validate_policy":"at_end"},
		"operations":[
		{"id":"consumer","action":"consume_asset","depends_on":["producer"],"specification":{"input_asset":{"$ref":"producer.result.primary_asset"}}},
		{"id":"producer","action":"produce_asset","specification":{}}
		],"on_failure":"rollback_all"}}
	)JSON");

	FString ResponseJson;
	TestTrue(
		TEXT("plan executes"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestTrue(TEXT("transaction began"), bBegan);
	TestTrue(TEXT("transaction committed"), bCommitted);
	TestFalse(TEXT("successful plan did not roll back"), bRolledBack);
	TestEqual(TEXT("topological dispatch count"), DispatchOrder.Num(), 2);
	TestEqual(TEXT("producer dispatched first"), DispatchOrder[0], FString(TEXT("producer")));
	TestEqual(TEXT("consumer dispatched second"), DispatchOrder[1], FString(TEXT("consumer")));

	const TSharedPtr<FJsonObject> Response = ParseJson(ResponseJson);
	TestTrue(TEXT("response parses"), Response.IsValid());
	TestEqual(
		TEXT("terminal validated status survives"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("modified_and_validated")));
	TestEqual(
		TEXT("terminal revision survives"),
		Response->GetStringField(TEXT("revision")),
		FString(TEXT("sha256:consumer")));
	const TSharedPtr<FJsonObject>* Result = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* OperationResults = nullptr;
	TestTrue(
		TEXT("consolidated operation results returned"),
		Response->TryGetObjectField(TEXT("result"), Result)
			&& Result
			&& (*Result)->TryGetArrayField(TEXT("operations"), OperationResults)
			&& OperationResults
			&& OperationResults->Num() == 2);
	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	TestTrue(TEXT("metrics returned"), Response->TryGetObjectField(TEXT("metrics"), Metrics));
	TestEqual(
		TEXT("internal operations sum"),
		static_cast<int32>((*Metrics)->GetNumberField(TEXT("internal_operations"))),
		8);
	TestEqual(
		TEXT("internal delegation stays one MCP call"),
		static_cast<int32>((*Metrics)->GetNumberField(TEXT("mcp_round_trips"))),
		1);
	const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
	TestTrue(
		TEXT("changes aggregate across operations"),
		Response->TryGetArrayField(TEXT("changes"), Changes)
			&& Changes
			&& Changes->Num() == 2);
	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanExecutorRollbackEvidenceTest,
	"UEREMCP.Protocol.PlanExecutor.RollbackEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanExecutorRollbackEvidenceTest::RunTest(const FString& Parameters)
{
	ResetExecutor();
	FString Error;
	TestTrue(
		TEXT("failing action registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("fail_action"),
			[](
				const FString&,
				FString& OutResponseJson,
				FString& OutError)
			{
				OutResponseJson = TEXT(R"JSON(
					{"protocol_version":"1.0","status":"failed_validation","summary":"expected failure",
					"metrics":{"mcp_round_trips":1,"internal_operations":2}}
				)JSON");
				OutError.Reset();
				return true;
			},
			Error));

	bool bRolledBack = false;
	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [](FString&) { return true; };
	Transaction.Commit = [](FString&) { return true; };
	Transaction.Rollback = [&bRolledBack](FString&)
	{
		bRolledBack = true;
		return true;
	};
	TestTrue(
		TEXT("transaction coordinator registers"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));

	const FString Request = TEXT(R"JSON(
		{"protocol_version":"1.0","request_id":"plan-rollback","action":"execute_plan",
		"specification":{"transaction":{"atomic":true,"rollback_on_failure":true},
		"operations":[{"id":"failure","action":"fail_action"}],"on_failure":"rollback_all"}}
	)JSON");
	FString ResponseJson;
	TestTrue(
		TEXT("failed plan returns structured response"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestTrue(TEXT("rollback callback ran"), bRolledBack);
	const TSharedPtr<FJsonObject> Response = ParseJson(ResponseJson);
	TestEqual(
		TEXT("rollback is reported honestly"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("rolled_back")));
	const TSharedPtr<FJsonObject>* Rollback = nullptr;
	TestTrue(
		TEXT("rollback evidence returned"),
		Response->TryGetObjectField(TEXT("rollback"), Rollback)
			&& Rollback
			&& (*Rollback)->GetBoolField(TEXT("performed")));
	ResetExecutor();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
