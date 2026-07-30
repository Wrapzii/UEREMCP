// Editor automation tests for UeremcpMaterial execute_plan handler registration (WS-08).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpMaterialPlanHandlers.h"
#include "UeremcpMaterialToolset.h"
#include "UeremcpPlanExecutor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpMaterialPlanHandlersTest
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
		"request_id":"ws08-plan-create-dry",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":false,"rollback_on_failure":false},
			"operations":[{
				"id":"core_material",
				"action":"create_vfx_material",
				"target":{"asset_path":"/Game/__UeremcpTests/Materials/MI_WS08_PlanDry"},
				"specification":{
					"purpose":"elemental_projectile_core",
					"element":"fire",
					"features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]
				}
			}],
			"on_failure":"stop"
		},
		"options":{"dry_run":true}
	})");

	const FString AtomicPlanRequest = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"ws08-plan-atomic-blocked",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":true,"rollback_on_failure":true},
			"operations":[{
				"id":"core_material",
				"action":"create_vfx_material",
				"target":{"asset_path":"/Game/__UeremcpTests/Materials/MI_WS08_PlanAtomic"},
				"specification":{
					"purpose":"elemental_projectile_core",
					"element":"fire",
					"features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]
				}
			}],
			"on_failure":"rollback_all"
		},
		"options":{"dry_run":true}
	})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialPlanHandlersRegistrationTest,
	"UEREMCP.Material.PlanHandlers.Registration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialPlanHandlersRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMaterialPlanHandlersTest;
	ResetExecutor();

	TestEqual(
		TEXT("registered action name"),
		FString(FUeremcpMaterialPlanHandlers::RegisteredActionName()),
		FString(TEXT("create_vfx_material")));

	FString Error;
	TestTrue(TEXT("register material plan handler"), FUeremcpMaterialPlanHandlers::Register(Error));

	FString DuplicateError;
	TestFalse(
		TEXT("duplicate create_vfx_material registration fails closed"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_vfx_material"),
			[](const FString&, FString& OutResponse, FString&)
			{
				OutResponse = TEXT("{}");
				return true;
			},
			DuplicateError));

	FString ResponseJson;
	TestTrue(
		TEXT("execute_plan dispatches registered create_vfx_material handler"),
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

	FUeremcpMaterialPlanHandlers::Unregister();

	FString MissingResponse;
	TestTrue(
		TEXT("preflight rejects missing handler after unregister"),
		FUeremcpPlanExecutor::ExecuteRequest(DryRunPlanRequest, MissingResponse, Error));
	TestTrue(TEXT("missing handler rejected"), MissingResponse.Contains(TEXT("rejected")));

	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialPlanHandlersAtomicPreflightTest,
	"UEREMCP.Material.PlanHandlers.AtomicPreflightBlocked",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialPlanHandlersAtomicPreflightTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMaterialPlanHandlersTest;
	ResetExecutor();

	FString Error;
	TestTrue(TEXT("register handler for atomic preflight"), FUeremcpMaterialPlanHandlers::Register(Error));

	FString ResponseJson;
	TestTrue(
		TEXT("atomic plan rejects before mutation without WS-03 callbacks"),
		FUeremcpPlanExecutor::ExecuteRequest(AtomicPlanRequest, ResponseJson, Error));
	TestTrue(TEXT("atomic rejection mentions transaction callbacks"), ResponseJson.Contains(TEXT("transaction callbacks")));
	TestTrue(TEXT("atomic response never claims validated"), ResponseNeverClaimsValidated(ResponseJson));

	FUeremcpMaterialPlanHandlers::Unregister();
	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialPlanHandlersCreateHonestyTest,
	"UEREMCP.Material.PlanHandlers.CreateHonestyDryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialPlanHandlersCreateHonestyTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMaterialPlanHandlersTest;

	const FString DirectRequest = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws08-plan-direct-dry","action":"create_vfx_material","target":{"asset_path":"/Game/__UeremcpTests/Materials/MI_WS08_PlanDirectDry"},"specification":{"purpose":"elemental_projectile_core","element":"fire","features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]},"options":{"dry_run":true}})");

	const FString DirectResponse = UUeremcpMaterialToolset::CreateVfxMaterial(DirectRequest);
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
