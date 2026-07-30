#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpGameplayToolset.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPermissionTier.h"
#include "UeremcpSpellPlanner.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TSharedPtr<FJsonObject> ParseObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

const TCHAR* ValidSpecificationJson = TEXT(R"({
	"name":"Fireball UEREMCP",
	"row_name":"Fireball_Ueremcp",
	"element":"Fire",
	"element_color":[1.0,0.12,0.01,1.0],
	"delivery":{"type":"projectile","speed":2400,"range":3200},
	"impact":{"damage":45,"aoe_radius":175,"status":"Burn","status_duration":3},
	"presentation":{
		"projectile_effect":"/Game/__UeremcpTests/VFX/NS_Fireball",
		"impact_effect":"/Game/__UeremcpTests/VFX/NS_Fireball_Impact"
	},
	"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"}
})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSpellPlannerValidTest,
	"UEREMCP.Gameplay.SpellPlanner.ValidRESpell",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSpellPlannerValidTest::RunTest(const FString& Parameters)
{
	FUeremcpSpellPlan Plan;
	FString Error;
	TestTrue(
		TEXT("valid create_spell specification plans"),
		FUeremcpSpellPlanner::BuildPlan(ParseObject(ValidSpecificationJson), Plan, Error));
	TestEqual(TEXT("stable row name"), Plan.RowName, FString(TEXT("Fireball_Ueremcp")));
	TestTrue(TEXT("row payload exists"), Plan.RowPayload.IsValid());
	if (Plan.RowPayload.IsValid())
	{
		static const TSet<FString> ExpectedFields = {
			TEXT("AbilityId"), TEXT("DisplayName"), TEXT("LineId"), TEXT("Element"),
			TEXT("Tier"), TEXT("Wheel"), TEXT("CastType"), TEXT("CircleTier"),
			TEXT("ElementColor"), TEXT("CastTimeSec"), TEXT("CooldownSec"),
			TEXT("StaminaCost"), TEXT("DurationSec"), TEXT("EffectTag"), TEXT("Speed"),
			TEXT("Range"), TEXT("ProjRadius"), TEXT("GravityScale"), TEXT("Homing"),
			TEXT("ImpactDamage"), TEXT("ImpactStatus"), TEXT("StatusDuration"),
			TEXT("AoeRadius"), TEXT("EscalateTo"), TEXT("SpawnEntity"),
			TEXT("EntityLengthCm"), TEXT("EntityThicknessCm"), TEXT("EntityHeightCm"),
			TEXT("CastNS"), TEXT("ProjectileNS"), TEXT("ImpactNS"),
			TEXT("CircleMaterial"), TEXT("VFXDefinition"), TEXT("CircleDiameterCm"),
			TEXT("AudioCueCast"), TEXT("AudioCueTravel"), TEXT("AudioCueImpact"),
			TEXT("AudioCueFail"), TEXT("UnlockSkillNode"), TEXT("MinClassification"),
		};
		TSet<FString> ActualFields;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Plan.RowPayload->Values)
		{
			ActualFields.Add(Pair.Key);
		}
		bool bExactFields = ActualFields.Num() == ExpectedFields.Num();
		for (const FString& ExpectedField : ExpectedFields)
		{
			bExactFields = bExactFields && ActualFields.Contains(ExpectedField);
		}
		TestTrue(TEXT("planner covers every verified FREAbilityDef field exactly"), bExactFields);
		TestEqual(
			TEXT("AbilityId echoes row identity"),
			Plan.RowPayload->GetStringField(TEXT("AbilityId")),
			FString(TEXT("Fireball_Ueremcp")));
		TestEqual(
			TEXT("semantic delivery maps to RE enum"),
			Plan.RowPayload->GetStringField(TEXT("CastType")),
			FString(TEXT("Projectile")));
		TestEqual(
			TEXT("RE status enum retained"),
			Plan.RowPayload->GetStringField(TEXT("ImpactStatus")),
			FString(TEXT("Burn")));
	}
	TestEqual(TEXT("both VFX dependencies returned"), Plan.DependencyAssetPaths.Num(), 2);

	FUeremcpAbilityTableWriteOptions WriteOptions;
	WriteOptions.RequestId = TEXT("ws09-write-plan");
	WriteOptions.Mode = TEXT("create_or_update");
	WriteOptions.bDryRun = true;
	WriteOptions.bAtomic = true;
	WriteOptions.bSave = true;
	WriteOptions.bValidate = true;
	WriteOptions.bRollbackOnFailure = true;
	WriteOptions.TimeoutMs = 120000;
	WriteOptions.OnRevisionConflict = TEXT("return_conflict");
	WriteOptions.ExpectedRevision = TEXT("sha256:expected");
	WriteOptions.bHasExpectedRevision = true;
	WriteOptions.IdempotencyKey = TEXT("ws09-fireball");
	FUeremcpAbilityTableWritePlan WritePlan;
	TestTrue(
		TEXT("guarded DataTable write plan builds"),
		FUeremcpSpellPlanner::BuildTableWritePlan(
			TEXT("/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"),
			WriteOptions,
			Plan,
			WritePlan,
			Error));
	TestEqual(
		TEXT("object path is deterministic without suffixing"),
		WritePlan.TableObjectPath,
		FString(TEXT("/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities.DT_UeremcpAbilities")));
	TestEqual(
		TEXT("RE row struct path is explicit"),
		WritePlan.RowStructPath,
		FString(TEXT("/Script/RE.REAbilityDef")));
	TestEqual(
		TEXT("dry run ends in discard step"),
		WritePlan.OrderedSteps.Last(),
		FString(TEXT("release_shared_mutator")));
	TestTrue(
		TEXT("dry run includes discard before release"),
		WritePlan.OrderedSteps.Contains(TEXT("discard_dry_run")));
	TestTrue(
		TEXT("expected revision is retained"),
		WritePlan.bHasExpectedRevision
			&& WritePlan.ExpectedRevision == TEXT("sha256:expected")
			&& WritePlan.OrderedSteps.Contains(TEXT("check_expected_revision")));
	TestTrue(
		TEXT("idempotency key is retained"),
		WritePlan.IdempotencyKey == TEXT("ws09-fireball")
			&& WritePlan.OrderedSteps.Contains(TEXT("check_idempotency_replay")));
	TestFalse(
		TEXT("dry run is never eligible for validated mutation status"),
		WritePlan.bCanClaimValidatedMutation);
	TestTrue(
		TEXT("queue and conflict controls are retained"),
		WritePlan.TimeoutMs == 120000
			&& WritePlan.OnRevisionConflict == TEXT("return_conflict")
			&& WritePlan.bAtomic
			&& WritePlan.bRollbackOnFailure);
	TestTrue(
		TEXT("only remaining runtime gate is shared Core dispatcher"),
		WritePlan.RequiredRuntimeGates.Num() == 1
			&& WritePlan.RequiredRuntimeGates[0] == TEXT("UeremcpCore.mutating_dispatcher"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSpellPlannerRejectsClientAuthorityTest,
	"UEREMCP.Gameplay.SpellPlanner.RejectsClientAuthority",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSpellPlannerRejectsClientAuthorityTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Specification = ParseObject(ValidSpecificationJson);
	const TSharedPtr<FJsonObject>* Networking = nullptr;
	if (Specification.IsValid()
		&& Specification->TryGetObjectField(TEXT("networking"), Networking)
		&& Networking && Networking->IsValid())
	{
		(*Networking)->SetStringField(TEXT("authority"), TEXT("client"));
	}

	FUeremcpSpellPlan Plan;
	FString Error;
	TestFalse(
		TEXT("client-authority declaration rejected"),
		FUeremcpSpellPlanner::BuildPlan(Specification, Plan, Error));
	TestTrue(TEXT("error identifies Pattern B"), Error.Contains(TEXT("Pattern B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSpellPlannerDefaultsAndInvariantsTest,
	"UEREMCP.Gameplay.SpellPlanner.DefaultsAndInvariants",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSpellPlannerDefaultsAndInvariantsTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Specification = ParseObject(ValidSpecificationJson);
	const TSharedPtr<FJsonObject>* Delivery = nullptr;
	if (Specification.IsValid()
		&& Specification->TryGetObjectField(TEXT("delivery"), Delivery)
		&& Delivery && Delivery->IsValid())
	{
		(*Delivery)->SetStringField(TEXT("type"), TEXT("self_cast"));
		(*Delivery)->RemoveField(TEXT("speed"));
	}
	Specification->RemoveField(TEXT("name"));
	Specification->SetStringField(TEXT("element"), TEXT("Dark Fire"));

	FUeremcpSpellPlan Plan;
	FString Error;
	TestTrue(
		TEXT("self cast with defaults plans"),
		FUeremcpSpellPlanner::BuildPlan(Specification, Plan, Error));
	if (Plan.RowPayload.IsValid())
	{
		TestEqual(
			TEXT("display name defaults to stable row name"),
			Plan.RowPayload->GetStringField(TEXT("DisplayName")),
			FString(TEXT("Fireball_Ueremcp")));
		TestEqual(
			TEXT("line id defaults deterministically from element"),
			Plan.RowPayload->GetStringField(TEXT("LineId")),
			FString(TEXT("dark_fire")));
		TestEqual(
			TEXT("non-projectile speed defaults to zero"),
			Plan.RowPayload->GetNumberField(TEXT("Speed")),
			0.0);
	}

	const TSharedPtr<FJsonObject>* Impact = nullptr;
	if (Specification->TryGetObjectField(TEXT("impact"), Impact)
		&& Impact && Impact->IsValid())
	{
		(*Impact)->SetNumberField(TEXT("status_duration"), 0.0);
	}
	TestFalse(
		TEXT("non-None status requires positive duration"),
		FUeremcpSpellPlanner::BuildPlan(Specification, Plan, Error));
	TestTrue(TEXT("duration rejection is actionable"), Error.Contains(TEXT("status_duration")));

	(*Impact)->SetNumberField(TEXT("status_duration"), 3.0);
	(*Delivery)->SetStringField(TEXT("speed"), TEXT("fast"));
	TestFalse(
		TEXT("wrong optional field type is rejected instead of defaulted"),
		FUeremcpSpellPlanner::BuildPlan(Specification, Plan, Error));
	TestTrue(TEXT("type rejection is actionable"), Error.Contains(TEXT("wrong JSON type")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSpellWritePlanControlsTest,
	"UEREMCP.Gameplay.SpellPlanner.WritePlanControls",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSpellWritePlanControlsTest::RunTest(const FString& Parameters)
{
	FUeremcpSpellPlan SpellPlan;
	FString Error;
	TestTrue(
		TEXT("valid spell plans before write controls"),
		FUeremcpSpellPlanner::BuildPlan(
			ParseObject(ValidSpecificationJson),
			SpellPlan,
			Error));

	FUeremcpAbilityTableWriteOptions Options;
	Options.Mode = TEXT("create");
	Options.bDryRun = false;
	Options.bAtomic = false;
	Options.bSave = false;
	Options.bValidate = true;
	Options.bRollbackOnFailure = false;
	Options.TimeoutMs = 5000;
	Options.OnRevisionConflict = TEXT("reject");
	FUeremcpAbilityTableWritePlan WritePlan;
	TestFalse(
		TEXT("request identity is mandatory for future queue ownership"),
		FUeremcpSpellPlanner::BuildTableWritePlan(
			TEXT("/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"),
			Options,
			SpellPlan,
			WritePlan,
			Error));
	TestTrue(TEXT("missing identity error is actionable"), Error.Contains(TEXT("request_id")));

	Options.RequestId = TEXT("ws09-write-controls");
	TestTrue(
		TEXT("no-save write intent plans honestly"),
		FUeremcpSpellPlanner::BuildTableWritePlan(
			TEXT("/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"),
			Options,
			SpellPlan,
			WritePlan,
			Error));
	TestTrue(
		TEXT("save=false is an explicit step"),
		WritePlan.OrderedSteps.Contains(TEXT("skip_save_requested")));
	TestTrue(
		TEXT("non-dry intent still persists sandbox"),
		WritePlan.OrderedSteps.Contains(TEXT("persist_sandbox")));
	TestTrue(
		TEXT("atomic and rollback controls are explicit"),
		WritePlan.OrderedSteps.Contains(TEXT("enter_best_effort_content_sandbox"))
			&& WritePlan.OrderedSteps.Contains(TEXT("configure_no_rollback_on_failure"))
			&& WritePlan.TimeoutMs == 5000);
	TestFalse(
		TEXT("unsaved intent cannot claim validated mutation"),
		WritePlan.bCanClaimValidatedMutation);

	Options.bSave = true;
	Options.bValidate = false;
	TestTrue(
		TEXT("no-validate write intent plans honestly"),
		FUeremcpSpellPlanner::BuildTableWritePlan(
			TEXT("/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"),
			Options,
			SpellPlan,
			WritePlan,
			Error));
	TestTrue(
		TEXT("validate=false is an explicit step"),
		WritePlan.OrderedSteps.Contains(TEXT("skip_validation_requested")));
	TestFalse(
		TEXT("unvalidated intent cannot claim validated mutation"),
		WritePlan.bCanClaimValidatedMutation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCreateSpellPreflightHonestyTest,
	"UEREMCP.Gameplay.Toolset.CreateSpellPreflightHonesty",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpCreateSpellPreflightHonestyTest::RunTest(const FString& Parameters)
{
	const FString Request = FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"ws09-preflight",
			"action":"create_spell",
			"mode":"create_or_update",
			"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"},
			"options":{"dry_run":true},
			"specification":%s
		})"),
		ValidSpecificationJson);

	const TSharedPtr<FJsonObject> Response =
		ParseObject(UUeremcpGameplayToolset::CreateSpell(Request));
	TestTrue(TEXT("response parses"), Response.IsValid());
	if (!Response.IsValid())
	{
		return false;
	}

	TestEqual(
		TEXT("no unverified mutation success"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));

	const TSharedPtr<FJsonObject>* Validation = nullptr;
	TestTrue(
		TEXT("validation evidence returned"),
		Response->TryGetObjectField(TEXT("validation"), Validation)
			&& Validation && Validation->IsValid());
	if (Validation && Validation->IsValid())
	{
		TestTrue(
			TEXT("static structure validated"),
			(*Validation)->GetBoolField(TEXT("structurally_valid")));
		TestTrue(
			TEXT("write reread explicitly unperformed"),
			(*Validation)->HasTypedField<EJson::Null>(TEXT("reread_after_write")));
	}

	const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
	TestTrue(
		TEXT("dry-run preflight returns explicit empty changes"),
		Response->TryGetArrayField(TEXT("changes"), Changes) && Changes && Changes->IsEmpty());

	const TSharedPtr<FJsonObject>* Rollback = nullptr;
	TestTrue(
		TEXT("rollback state is explicit"),
		Response->TryGetObjectField(TEXT("rollback"), Rollback)
			&& Rollback && Rollback->IsValid());
	if (Rollback && Rollback->IsValid())
	{
		TestFalse(TEXT("rollback not falsely available"), (*Rollback)->GetBoolField(TEXT("available")));
		TestFalse(TEXT("rollback not falsely performed"), (*Rollback)->GetBoolField(TEXT("performed")));
		TestEqual(
			TEXT("rollback scope is none"),
			(*Rollback)->GetStringField(TEXT("scope")),
			FString(TEXT("none")));
	}

	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	TestTrue(
		TEXT("dry-run preflight includes execution trace"),
		Response->TryGetObjectField(TEXT("diagnostics"), Diagnostics)
			&& Diagnostics && Diagnostics->IsValid()
			&& (*Diagnostics)->HasField(TEXT("execution_trace")));

	const TSharedPtr<FJsonObject>* Result = nullptr;
	if (Response->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid())
	{
		TestFalse(TEXT("preflight does not claim created assets"), (*Result)->HasField(TEXT("created_assets")));
		TestFalse(TEXT("preflight does not claim modified assets"), (*Result)->HasField(TEXT("modified_assets")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCreateSpellQueueGateLifecycleTest,
	"UEREMCP.Gameplay.Toolset.CreateSpellQueueGateLifecycle",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpCreateSpellQueueGateLifecycleTest::RunTest(const FString& Parameters)
{
	const FString ProjectPath =
		FPaths::GetProjectFilePath().Replace(TEXT("\\"), TEXT("\\\\"));
	const FString Request = FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"ws09-queue-lifecycle",
			"action":"create_spell",
			"project":{"path":"%s","engine_version":"5.8"},
			"mode":"create_or_update",
			"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"},
			"options":{"dry_run":false},
			"specification":%s
		})"),
		*ProjectPath,
		ValidSpecificationJson);

	const TSharedPtr<FJsonObject> Response =
		ParseObject(UUeremcpGameplayToolset::CreateSpell(Request));
	TestTrue(TEXT("queue-gated response parses"), Response.IsValid());
	if (Response.IsValid())
	{
		TestEqual(
			TEXT("no mutation success is claimed while Core gate is open"),
			Response->GetStringField(TEXT("status")),
			FString(TEXT("partially_completed")));
		const TSharedPtr<FJsonObject>* Validation = nullptr;
		if (Response->TryGetObjectField(TEXT("validation"), Validation)
			&& Validation && Validation->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Checks = nullptr;
			if ((*Validation)->TryGetArrayField(TEXT("checks_performed"), Checks) && Checks)
			{
				TArray<FString> CheckNames;
				for (const TSharedPtr<FJsonValue>& Check : *Checks)
				{
					CheckNames.Add(Check->AsString());
				}
				TestTrue(
					TEXT("queue acquisition recorded"),
					CheckNames.Contains(TEXT("mutator_queue_acquired")));
				TestTrue(
					TEXT("terminal audit recorded"),
					CheckNames.Contains(TEXT("terminal_audit_appended")));
				TestTrue(
					TEXT("queue release recorded"),
					CheckNames.Contains(TEXT("mutator_queue_released")));
			}
		}
	}
	TestFalse(
		TEXT("request never leaks queue ownership"),
		FUeremcpMutatorQueue::IsActive(FPaths::GetProjectFilePath()));

	const FUeremcpMutatorQueue::FAcquireResult Blocker =
		FUeremcpMutatorQueue::TryAcquire(
			FPaths::GetProjectFilePath(),
			TEXT("ws09-blocker"),
			EUeremcpPermissionTier::Write);
	TestTrue(TEXT("test blocker acquires queue"), Blocker.bAcquired);
	const FString ContendedRequest =
		Request.Replace(TEXT("ws09-queue-lifecycle"), TEXT("ws09-queued-cancel"));
	const TSharedPtr<FJsonObject> ContendedResponse =
		ParseObject(UUeremcpGameplayToolset::CreateSpell(ContendedRequest));
	TestTrue(TEXT("contended response parses"), ContendedResponse.IsValid());
	if (ContendedResponse.IsValid())
	{
		TestTrue(
			TEXT("contended request fails closed"),
			ContendedResponse->GetStringField(TEXT("summary")).Contains(TEXT("no write occurred")));
	}
	TestEqual(
		TEXT("terminal rejection cancels abandoned waiter"),
		FUeremcpMutatorQueue::PendingCount(FPaths::GetProjectFilePath()),
		0);
	TestTrue(
		TEXT("test blocker releases queue"),
		FUeremcpMutatorQueue::Release(
			FPaths::GetProjectFilePath(),
			TEXT("ws09-blocker")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
