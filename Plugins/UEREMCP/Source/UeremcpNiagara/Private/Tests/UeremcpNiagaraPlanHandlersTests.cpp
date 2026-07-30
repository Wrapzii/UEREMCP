// Editor automation tests for UeremcpNiagara execute_plan handler registration (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraPlanHandlers.h"
#include "UeremcpNiagaraToolset.h"
#include "UeremcpPlanExecutor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpNiagaraPlanHandlersTest
{
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
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

	bool ResponseNeverClaimsValidated(const FString& ResponseJson)
	{
		return !ResponseJson.Contains(TEXT("created_and_validated"))
			&& !ResponseJson.Contains(TEXT("modified_and_validated"));
	}

	const FString DryRunPlanRequest = TEXT(R"({
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

	const FString AtomicPlanRequest = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"ws07-plan-atomic-blocked",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":true,"rollback_on_failure":true},
			"operations":[{
				"id":"niagara_create",
				"action":"create_niagara_effect",
				"target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_PlanAtomic"},
				"specification":{
					"effect_type":"projectile",
					"element":"fire",
					"components":["sparks"],
					"parameters":{"scale":1.0,"intensity":4.0}
				}
			}],
			"on_failure":"rollback_all"
		},
		"options":{"dry_run":true}
	})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPlanHandlersRegistrationTest,
	"UEREMCP.Niagara.PlanHandlers.Registration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPlanHandlersRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraPlanHandlersTest;
	ResetExecutor();

	TestEqual(
		TEXT("registered action name"),
		FString(FUeremcpNiagaraPlanHandlers::RegisteredActionName()),
		FString(TEXT("create_niagara_effect")));

	FString Error;
	TestTrue(TEXT("register niagara plan handler"), FUeremcpNiagaraPlanHandlers::Register(Error));

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

	FString ResponseJson;
	TestTrue(
		TEXT("execute_plan dispatches registered create_niagara_effect handler"),
		FUeremcpPlanExecutor::ExecuteRequest(DryRunPlanRequest, ResponseJson, Error));
	TestTrue(TEXT("plan response never claims validated"), ResponseNeverClaimsValidated(ResponseJson));

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(ResponseJson);
	TestTrue(TEXT("plan response parsed"), Response.IsValid());
	FString AggregateStatus;
	TestTrue(TEXT("aggregate status present"), Response->TryGetStringField(TEXT("status"), AggregateStatus));
	TestEqual(TEXT("dry-run aggregate status"), AggregateStatus, FString(TEXT("no_change_required")));

	const TSharedPtr<FJsonObject>* ResultObject = nullptr;
	TestTrue(TEXT("result object present"), Response->TryGetObjectField(TEXT("result"), ResultObject));
	const TArray<TSharedPtr<FJsonValue>>* OperationResults = nullptr;
	TestTrue(
		TEXT("operation results present"),
		ResultObject && (*ResultObject)->TryGetArrayField(TEXT("operations"), OperationResults));
	TestTrue(TEXT("one operation result"), OperationResults && OperationResults->Num() == 1);

	const TSharedPtr<FJsonObject> OperationResult = (*OperationResults)[0]->AsObject();
	FString OperationStatus;
	TestTrue(
		TEXT("operation status present"),
		OperationResult.IsValid() && OperationResult->TryGetStringField(TEXT("status"), OperationStatus));
	TestEqual(TEXT("dry-run operation status"), OperationStatus, FString(TEXT("no_change_required")));

	FUeremcpNiagaraPlanHandlers::Unregister();

	FString MissingResponse;
	TestTrue(
		TEXT("preflight rejects missing handler after unregister"),
		FUeremcpPlanExecutor::ExecuteRequest(DryRunPlanRequest, MissingResponse, Error));
	TestTrue(TEXT("missing handler rejected"), MissingResponse.Contains(TEXT("rejected")));

	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPlanHandlersAtomicPreflightTest,
	"UEREMCP.Niagara.PlanHandlers.AtomicPreflightBlocked",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPlanHandlersAtomicPreflightTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraPlanHandlersTest;
	ResetExecutor();

	FString Error;
	TestTrue(TEXT("register handler for atomic preflight"), FUeremcpNiagaraPlanHandlers::Register(Error));

	FString ResponseJson;
	TestTrue(
		TEXT("atomic plan rejects before mutation without WS-03 callbacks"),
		FUeremcpPlanExecutor::ExecuteRequest(AtomicPlanRequest, ResponseJson, Error));
	TestTrue(TEXT("atomic rejection mentions transaction callbacks"), ResponseJson.Contains(TEXT("transaction callbacks")));
	TestTrue(TEXT("atomic response never claims validated"), ResponseNeverClaimsValidated(ResponseJson));

	FUeremcpNiagaraPlanHandlers::Unregister();
	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPlanHandlersCreateHonestyTest,
	"UEREMCP.Niagara.PlanHandlers.CreateHonestyDryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPlanHandlersCreateHonestyTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraPlanHandlersTest;

	const FString DirectRequest = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-plan-direct-dry","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_PlanDirectDry"},"specification":{"effect_type":"projectile","element":"fire","components":["sparks"],"parameters":{"scale":1.0,"intensity":4.0}},"options":{"dry_run":true}})");

	const FString DirectResponse = UUeremcpNiagaraToolset::CreateNiagaraEffect(DirectRequest);
	TestFalse(TEXT("direct create response empty"), DirectResponse.IsEmpty());
	TestTrue(TEXT("direct create never claims validated"), ResponseNeverClaimsValidated(DirectResponse));

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(DirectResponse);
	TestTrue(TEXT("direct create JSON parsed"), Response.IsValid());

	FString Status;
	TestTrue(TEXT("direct create status"), Response->TryGetStringField(TEXT("status"), Status));
	TestEqual(TEXT("direct dry-run status"), Status, FString(TEXT("no_change_required")));

	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	TestTrue(TEXT("metrics object present"), Response->TryGetObjectField(TEXT("metrics"), Metrics));
	double InternalOperations = -1.0;
	TestTrue(
		TEXT("internal_operations present for plan executor"),
		Metrics && (*Metrics)->TryGetNumberField(TEXT("internal_operations"), InternalOperations));
	TestTrue(TEXT("internal_operations non-negative"), InternalOperations >= 0.0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
