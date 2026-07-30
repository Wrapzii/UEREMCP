// Editor automation tests for UeremcpNiagara execute_plan handler registration (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraPlanHandlers.h"
#include "UeremcpPlanExecutor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPlanHandlersRegistrationTest,
	"UEREMCP.Niagara.PlanHandlers.Registration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPlanHandlersRegistrationTest::RunTest(const FString& Parameters)
{
	FUeremcpPlanExecutor::ClearActionHandlers();
	FUeremcpPlanExecutor::ClearTransactionCallbacks();

	FString Error;
	TestTrue(TEXT("register niagara plan handlers"), FUeremcpNiagaraPlanHandlers::Register(Error));

	FString DuplicateError;
	TestFalse(
		TEXT("duplicate create_niagara_effect registration fails closed"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_niagara_effect"),
			[](const FString&, FString& OutResponse, FString&)
			{
				OutResponse = TEXT("{}");
				return true;
			},
			DuplicateError));

	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"ws07-plan-create-dry",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":false,"rollback_on_failure":false},
			"operations":[{
				"id":"niagara_create",
				"action":"create_niagara_effect",
				"target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_PlanDry"},
				"specification":{
					"effect_type":"projectile",
					"element":"fire",
					"components":["sparks"],
					"parameters":{"scale":1.0,"intensity":4.0}
				}
			}],
			"on_failure":"stop"
		},
		"options":{"dry_run":true}
	})");

	FString ResponseJson;
	TestTrue(
		TEXT("execute_plan dispatches registered create_niagara_effect handler"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(ResponseJson);
	TestTrue(TEXT("plan response parsed"), Response.IsValid());
	FString Status;
	TestTrue(TEXT("plan response status"), Response->TryGetStringField(TEXT("status"), Status));
	TestNotEqual(
		TEXT("not rejected for missing handler"),
		Status,
		FString(TEXT("rejected")));

	FUeremcpNiagaraPlanHandlers::Unregister();

	const FString MissingHandlerPlan = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"ws07-plan-missing",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":false,"rollback_on_failure":false},
			"operations":[{"id":"niagara","action":"create_niagara_effect"}],
			"on_failure":"stop"
		}
	})");
	FString MissingResponse;
	TestTrue(
		TEXT("preflight rejects missing handler after unregister"),
		FUeremcpPlanExecutor::ExecuteRequest(MissingHandlerPlan, MissingResponse, Error));
	TestTrue(
		TEXT("missing handler response"),
		MissingResponse.Contains(TEXT("rejected")));

	FUeremcpPlanExecutor::ClearActionHandlers();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
