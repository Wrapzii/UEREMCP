// Editor automation tests for UeremcpGameplay execute_plan + POC D (WS-09).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpGameplayPlanHandlers.h"
#include "UeremcpGameplayToolset.h"
#include "UeremcpIdempotency.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPlanExecutor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpGameplayPlanHandlersTest
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

FString MakeStubSuccess(
	const FString& Status,
	const FString& Summary,
	const FString& PrimaryAsset)
{
	return FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"stub",
			"status":"%s",
			"summary":"%s",
			"understood":{"action":"stub"},
			"result":{"primary_asset":"%s"},
			"metrics":{"mcp_round_trips":0,"internal_operations":1,"assets_affected":1}
		})"),
		*Status,
		*Summary,
		*PrimaryAsset);
}

const TCHAR* SpellSpecNoPresentation = TEXT(R"({
	"name":"Fireball POC D",
	"row_name":"Fireball_PocD",
	"element":"Fire",
	"element_color":[1.0,0.12,0.01,1.0],
	"delivery":{"type":"projectile","speed":2400,"range":3200},
	"impact":{"damage":45,"aoe_radius":175,"status":"Burn","status_duration":3},
	"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"}
})");

const FString DryRunSpellPlanRequest = TEXT(R"({
	"protocol_version":"1.0",
	"request_id":"ws09-poc-d-plan-dry",
	"action":"execute_plan",
	"specification":{
		"transaction":{"atomic":false,"rollback_on_failure":false},
		"operations":[{
			"id":"spell",
			"action":"create_spell",
			"mode":"create_or_update",
			"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_PocD_PlanDry"},
			"specification":{
				"name":"Fireball POC D Dry",
				"row_name":"Fireball_PocD_Dry",
				"element":"Fire",
				"element_color":[1.0,0.12,0.01,1.0],
				"delivery":{"type":"projectile","speed":2400,"range":3200},
				"impact":{"damage":45,"aoe_radius":175,"status":"Burn","status_duration":3},
				"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"}
			}
		}],
		"on_failure":"stop"
	},
	"options":{"dry_run":true}
})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpGameplayPlanHandlersRegistrationTest,
	"UEREMCP.Gameplay.PlanHandlers.Registration",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpGameplayPlanHandlersRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpGameplayPlanHandlersTest;
	ResetExecutor();

	TestEqual(
		TEXT("registered action name"),
		FString(FUeremcpGameplayPlanHandlers::RegisteredActionName()),
		FString(TEXT("create_spell")));

	FString Error;
	TestTrue(TEXT("register gameplay plan handler"), FUeremcpGameplayPlanHandlers::Register(Error));

	FString DuplicateError;
	TestFalse(
		TEXT("duplicate create_spell registration fails closed"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_spell"),
			[](const FString&, FString& OutResponse, FString&)
			{
				OutResponse = TEXT("{}");
				return true;
			},
			DuplicateError));

	FString ResponseJson;
	TestTrue(
		TEXT("execute_plan dispatches registered create_spell handler"),
		FUeremcpPlanExecutor::ExecuteRequest(DryRunSpellPlanRequest, ResponseJson, Error));

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(ResponseJson);
	TestTrue(TEXT("plan response parsed"), Response.IsValid());
	if (Response.IsValid())
	{
		// Dry-run create_spell is intentionally not IsSuccess for plan deps; aggregate
		// remains partially_completed rather than a false *_validated claim.
		TestEqual(
			TEXT("dry-run aggregate status is honest"),
			Response->GetStringField(TEXT("status")),
			FString(TEXT("partially_completed")));

		const TSharedPtr<FJsonObject>* ResultObject = nullptr;
		TestTrue(TEXT("result object present"), Response->TryGetObjectField(TEXT("result"), ResultObject));
		const TArray<TSharedPtr<FJsonValue>>* OperationResults = nullptr;
		TestTrue(
			TEXT("operation results present"),
			ResultObject && (*ResultObject)->TryGetArrayField(TEXT("operations"), OperationResults));
		TestTrue(TEXT("one operation result"), OperationResults && OperationResults->Num() == 1);
		if (OperationResults && OperationResults->Num() == 1)
		{
			const TSharedPtr<FJsonObject> OperationResult = (*OperationResults)[0]->AsObject();
			TestEqual(
				TEXT("dry-run operation status"),
				OperationResult->GetStringField(TEXT("status")),
				FString(TEXT("partially_completed")));
			TestEqual(
				TEXT("operation action"),
				OperationResult->GetStringField(TEXT("action")),
				FString(TEXT("create_spell")));
		}
	}

	FUeremcpGameplayPlanHandlers::Unregister();

	FString MissingResponse;
	TestTrue(
		TEXT("preflight rejects missing handler after unregister"),
		FUeremcpPlanExecutor::ExecuteRequest(DryRunSpellPlanRequest, MissingResponse, Error));
	TestTrue(TEXT("missing handler rejected"), MissingResponse.Contains(TEXT("rejected")));

	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpGameplayPocDDependsOnAndRefTest,
	"UEREMCP.Gameplay.PocD.DependsOnAndRef",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpGameplayPocDDependsOnAndRefTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpGameplayPlanHandlersTest;
	ResetExecutor();

	FString Error;
	TArray<FString> ExecutionOrder;
	FString CapturedSpellRequest;

	// Non-atomic: dry_run create_spell returns partially_completed (honest, not
	// IsSuccess). An atomic plan would roll back that honesty signal.
	TestTrue(
		TEXT("stub material registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_vfx_material"),
			[&ExecutionOrder](const FString&, FString& OutResponse, FString&)
			{
				ExecutionOrder.Add(TEXT("core_material"));
				OutResponse = MakeStubSuccess(
					TEXT("created_and_validated"),
					TEXT("stub material"),
					TEXT("/Game/__UeremcpTests/VFX/Materials/MI_Fireball_Core_PocD"));
				return true;
			},
			Error));
	TestTrue(
		TEXT("stub niagara registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_niagara_effect"),
			[&ExecutionOrder](const FString& RequestJson, FString& OutResponse, FString&)
			{
				ExecutionOrder.Add(TEXT("projectile_fx"));
				const TSharedPtr<FJsonObject> Request = ParseJsonObject(RequestJson);
				const TSharedPtr<FJsonObject>* Spec = nullptr;
				FString CoreMaterial;
				const bool bResolved =
					Request.IsValid()
					&& Request->TryGetObjectField(TEXT("specification"), Spec)
					&& Spec && Spec->IsValid()
					&& (*Spec)->TryGetStringField(TEXT("core_material"), CoreMaterial)
					&& CoreMaterial == TEXT("/Game/__UeremcpTests/VFX/Materials/MI_Fireball_Core_PocD");
				OutResponse = MakeStubSuccess(
					bResolved ? TEXT("created_and_validated") : TEXT("failed_validation"),
					bResolved ? TEXT("stub niagara") : TEXT("material $ref unresolved"),
					TEXT("/Game/__UeremcpTests/VFX/Spells/NS_Fireball_PocD"));
				return bResolved;
			},
			Error));
	TestTrue(
		TEXT("create_spell wrapper registers"),
		FUeremcpPlanExecutor::RegisterAction(
			TEXT("create_spell"),
			[&ExecutionOrder, &CapturedSpellRequest](
				const FString& RequestJson,
				FString& OutResponse,
				FString&)
			{
				ExecutionOrder.Add(TEXT("spell"));
				CapturedSpellRequest = RequestJson;
				OutResponse = UUeremcpGameplayToolset::CreateSpell(RequestJson);
				return !OutResponse.IsEmpty();
			},
			Error));

	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"ws09-poc-d-ref",
		"action":"execute_plan",
		"specification":{
			"transaction":{"atomic":false,"rollback_on_failure":false},
			"operations":[
				{
					"id":"core_material",
					"action":"create_vfx_material",
					"target":{"asset_path":"/Game/__UeremcpTests/VFX/Materials/MI_Fireball_Core_PocD"}
				},
				{
					"id":"projectile_fx",
					"action":"create_niagara_effect",
					"depends_on":["core_material"],
					"target":{"asset_path":"/Game/__UeremcpTests/VFX/Spells/NS_Fireball_PocD"},
					"specification":{
						"core_material":{"$ref":"core_material.result.primary_asset"}
					}
				},
				{
					"id":"spell",
					"action":"create_spell",
					"depends_on":["projectile_fx"],
					"mode":"create_or_update",
					"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_PocD_Ref"},
					"specification":{
						"name":"Fireball POC D Ref",
						"row_name":"Fireball_PocD_Ref",
						"element":"Fire",
						"element_color":[1.0,0.12,0.01,1.0],
						"delivery":{"type":"projectile","speed":2400,"range":3200},
						"impact":{"damage":45,"aoe_radius":175,"status":"Burn","status_duration":3},
						"presentation":{
							"projectile_effect":{"$ref":"projectile_fx.result.primary_asset"}
						},
						"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"}
					}
				}
			],
			"on_failure":"stop"
		},
		"options":{"dry_run":true}
	})");

	FString ResponseJson;
	TestTrue(
		TEXT("batched POC D dry-run plan executes"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestEqual(TEXT("dependency order preserved"), ExecutionOrder.Num(), 3);
	if (ExecutionOrder.Num() == 3)
	{
		TestEqual(TEXT("material first"), ExecutionOrder[0], FString(TEXT("core_material")));
		TestEqual(TEXT("niagara second"), ExecutionOrder[1], FString(TEXT("projectile_fx")));
		TestEqual(TEXT("spell third"), ExecutionOrder[2], FString(TEXT("spell")));
	}

	TestTrue(TEXT("spell request captured"), !CapturedSpellRequest.IsEmpty());
	const TSharedPtr<FJsonObject> SpellRequest = ParseJsonObject(CapturedSpellRequest);
	TestTrue(TEXT("spell request parsed"), SpellRequest.IsValid());
	if (SpellRequest.IsValid())
	{
		const TSharedPtr<FJsonObject>* Spec = nullptr;
		TestTrue(
			TEXT("spell specification present"),
			SpellRequest->TryGetObjectField(TEXT("specification"), Spec)
				&& Spec && Spec->IsValid());
		if (Spec && Spec->IsValid())
		{
			TestEqual(
				TEXT("RE Element identity retained"),
				(*Spec)->GetStringField(TEXT("element")),
				FString(TEXT("Fire")));
			const TSharedPtr<FJsonObject>* Impact = nullptr;
			TestTrue(
				TEXT("impact block present"),
				(*Spec)->TryGetObjectField(TEXT("impact"), Impact)
					&& Impact && Impact->IsValid());
			if (Impact && Impact->IsValid())
			{
				TestEqual(
					TEXT("RE ImpactStatus uses Burn"),
					(*Impact)->GetStringField(TEXT("status")),
					FString(TEXT("Burn")));
			}
			const TSharedPtr<FJsonObject>* Networking = nullptr;
			TestTrue(
				TEXT("Pattern B networking present"),
				(*Spec)->TryGetObjectField(TEXT("networking"), Networking)
					&& Networking && Networking->IsValid());
			if (Networking && Networking->IsValid())
			{
				TestEqual(
					TEXT("Pattern B pattern"),
					(*Networking)->GetStringField(TEXT("pattern")),
					FString(TEXT("B")));
				TestEqual(
					TEXT("Pattern B cast path"),
					(*Networking)->GetStringField(TEXT("cast_path")),
					FString(TEXT("AuthorityCastAbility")));
			}
			const TSharedPtr<FJsonObject>* Presentation = nullptr;
			TestTrue(
				TEXT("presentation present after $ref"),
				(*Spec)->TryGetObjectField(TEXT("presentation"), Presentation)
					&& Presentation && Presentation->IsValid());
			if (Presentation && Presentation->IsValid())
			{
				TestEqual(
					TEXT("$ref substituted projectile effect"),
					(*Presentation)->GetStringField(TEXT("projectile_effect")),
					FString(TEXT("/Game/__UeremcpTests/VFX/Spells/NS_Fireball_PocD")));
			}
		}
	}

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(ResponseJson);
	TestTrue(TEXT("consolidated response parsed"), Response.IsValid());
	if (Response.IsValid())
	{
		const TSharedPtr<FJsonObject>* Result = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		TestTrue(
			TEXT("per-operation results present"),
			Response->TryGetObjectField(TEXT("result"), Result)
				&& Result && Result->IsValid()
				&& (*Result)->TryGetArrayField(TEXT("operations"), Ops)
				&& Ops && Ops->Num() == 3);
	}

	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpGameplayPocDLiveUpsertViaPlanTest,
	"UEREMCP.Gameplay.PocD.LiveUpsertViaPlan",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpGameplayPocDLiveUpsertViaPlanTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpGameplayPlanHandlersTest;
	ResetExecutor();
	FUeremcpIdempotencyStore::Get().Clear();

	FString Error;
	bool bBegin = false;
	bool bCommit = false;
	bool bRollback = false;
	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [&bBegin](FString&) { bBegin = true; return true; };
	Transaction.Commit = [&bCommit](FString&) { bCommit = true; return true; };
	Transaction.Rollback = [&bRollback](FString&) { bRollback = true; return true; };
	TestTrue(
		TEXT("transaction callbacks register"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));
	TestTrue(TEXT("create_spell registers"), FUeremcpGameplayPlanHandlers::Register(Error));

	const FString ProjectPath =
		FPaths::GetProjectFilePath().Replace(TEXT("\\"), TEXT("\\\\"));
	const FString Request = FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"ws09-poc-d-live",
			"action":"execute_plan",
			"project":{"path":"%s","engine_version":"5.8"},
			"specification":{
				"transaction":{"atomic":true,"rollback_on_failure":true,"validate_policy":"at_end"},
				"operations":[{
					"id":"spell",
					"action":"create_spell",
					"mode":"create_or_update",
					"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_PocD_Live"},
					"specification":%s
				}],
				"on_failure":"rollback_all"
			},
			"options":{"dry_run":false},
			"idempotency_key":"ws09-poc-d-live-upsert"
		})"),
		*ProjectPath,
		SpellSpecNoPresentation);

	FString ResponseJson;
	TestTrue(
		TEXT("live execute_plan create_spell runs"),
		FUeremcpPlanExecutor::ExecuteRequest(Request, ResponseJson, Error));
	TestTrue(TEXT("transaction began"), bBegin);

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(ResponseJson);
	TestTrue(TEXT("live plan response parsed"), Response.IsValid());
	if (Response.IsValid())
	{
		const FString Status = Response->GetStringField(TEXT("status"));
		TestTrue(
			TEXT("live upsert reports verified create/modify/no-change"),
			Status == TEXT("created_and_validated")
				|| Status == TEXT("modified_and_validated")
				|| Status == TEXT("no_change_required"));
		TestTrue(
			TEXT("successful path commits rather than rolls back"),
			bCommit && !bRollback);

		const TSharedPtr<FJsonObject>* Result = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		TestTrue(
			TEXT("consolidated per-operation results"),
			Response->TryGetObjectField(TEXT("result"), Result)
				&& Result && Result->IsValid()
				&& (*Result)->TryGetArrayField(TEXT("operations"), Ops)
				&& Ops && Ops->Num() == 1);
		if (Ops && Ops->Num() == 1)
		{
			TestEqual(
				TEXT("spell op status matches aggregate"),
				(*Ops)[0]->AsObject()->GetStringField(TEXT("status")),
				Status);
		}
	}
	TestFalse(
		TEXT("live plan never leaks mutator ownership"),
		FUeremcpMutatorQueue::IsActive(FPaths::GetProjectFilePath()));

	ResetExecutor();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpGameplayPocDRollbackOnFailureTest,
	"UEREMCP.Gameplay.PocD.RollbackOnFailure",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpGameplayPocDRollbackOnFailureTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpGameplayPlanHandlersTest;
	ResetExecutor();
	FUeremcpIdempotencyStore::Get().Clear();

	FString Error;
	bool bBegin = false;
	bool bCommit = false;
	bool bRollback = false;
	FUeremcpPlanTransactionCallbacks Transaction;
	Transaction.Begin = [&bBegin](FString&) { bBegin = true; return true; };
	Transaction.Commit = [&bCommit](FString&) { bCommit = true; return true; };
	Transaction.Rollback = [&bRollback](FString&) { bRollback = true; return true; };
	TestTrue(
		TEXT("transaction callbacks register"),
		FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Transaction), Error));
	TestTrue(TEXT("create_spell registers"), FUeremcpGameplayPlanHandlers::Register(Error));

	const FString ProjectPath =
		FPaths::GetProjectFilePath().Replace(TEXT("\\"), TEXT("\\\\"));

	// Seed a row so a subsequent mode=create collides and fails closed.
	const FString SeedRequest = FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"ws09-poc-d-rollback-seed",
			"action":"create_spell",
			"project":{"path":"%s","engine_version":"5.8"},
			"mode":"create_or_update",
			"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_PocD_Rollback"},
			"options":{"dry_run":false},
			"idempotency_key":"ws09-poc-d-rollback-seed",
			"specification":%s
		})"),
		*ProjectPath,
		SpellSpecNoPresentation);
	const TSharedPtr<FJsonObject> SeedResponse =
		ParseJsonObject(UUeremcpGameplayToolset::CreateSpell(SeedRequest));
	TestTrue(TEXT("seed response parses"), SeedResponse.IsValid());
	if (SeedResponse.IsValid())
	{
		const FString SeedStatus = SeedResponse->GetStringField(TEXT("status"));
		TestTrue(
			TEXT("seed row verified"),
			SeedStatus == TEXT("created_and_validated")
				|| SeedStatus == TEXT("modified_and_validated")
				|| SeedStatus == TEXT("no_change_required"));
	}

	const FString FailPlan = FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"ws09-poc-d-rollback",
			"action":"execute_plan",
			"project":{"path":"%s","engine_version":"5.8"},
			"specification":{
				"transaction":{"atomic":true,"rollback_on_failure":true},
				"operations":[{
					"id":"spell",
					"action":"create_spell",
					"mode":"create",
					"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_PocD_Rollback"},
					"specification":%s
				}],
				"on_failure":"rollback_all"
			},
			"options":{"dry_run":false},
			"idempotency_key":"ws09-poc-d-rollback-fail"
		})"),
		*ProjectPath,
		SpellSpecNoPresentation);

	FString ResponseJson;
	TestTrue(
		TEXT("failing execute_plan runs"),
		FUeremcpPlanExecutor::ExecuteRequest(FailPlan, ResponseJson, Error));
	TestTrue(TEXT("transaction began"), bBegin);
	TestTrue(TEXT("deliberate failure triggered plan rollback"), bRollback);
	TestFalse(TEXT("failed plan did not commit"), bCommit);

	const TSharedPtr<FJsonObject> Response = ParseJsonObject(ResponseJson);
	TestTrue(TEXT("rollback response parsed"), Response.IsValid());
	if (Response.IsValid())
	{
		TestEqual(
			TEXT("aggregate status is rolled_back"),
			Response->GetStringField(TEXT("status")),
			FString(TEXT("rolled_back")));
		const TSharedPtr<FJsonObject>* Result = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		TestTrue(
			TEXT("failed op reported in consolidated results"),
			Response->TryGetObjectField(TEXT("result"), Result)
				&& Result && Result->IsValid()
				&& (*Result)->TryGetArrayField(TEXT("operations"), Ops)
				&& Ops && Ops->Num() == 1);
		if (Ops && Ops->Num() == 1)
		{
			TestEqual(
				TEXT("create collision reports failed_validation"),
				(*Ops)[0]->AsObject()->GetStringField(TEXT("status")),
				FString(TEXT("failed_validation")));
		}
	}
	TestFalse(
		TEXT("rollback path releases mutator"),
		FUeremcpMutatorQueue::IsActive(FPaths::GetProjectFilePath()));

	ResetExecutor();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
