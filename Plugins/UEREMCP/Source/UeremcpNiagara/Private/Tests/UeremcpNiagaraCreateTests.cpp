// Editor automation tests for UeremcpNiagara create (WS-07).

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreatePathGuardTest,
	"UEREMCP.Niagara.Create.PathGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreatePathGuardTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("tests root allowed"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/__UeremcpTests/NS_WS07_CreateProbe")));
	TestTrue(TEXT("poc root allowed"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/__UeremcpPoc/Fireball/NS_Fireball")));
	TestTrue(TEXT("poc root exact allowed"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/__UeremcpPoc")));
	TestFalse(TEXT("game content rejected"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/VFX/NS_Fireball")));
	TestFalse(TEXT("tests prefix trick rejected"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/__UeremcpTestsEvil/NS_Probe")));
	TestFalse(TEXT("poc prefix trick rejected"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/__UeremcpPocExtra/NS_Fireball")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreateDryRunTest,
	"UEREMCP.Niagara.Create.DryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-create-dry","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_CreateDry"},"specification":{"effect_type":"projectile","element":"fire","components":["sparks"],"parameters":{"scale":1.0,"intensity":4.0}},"options":{"dry_run":true}})");

	const FString Json = UUeremcpNiagaraToolset::CreateNiagaraEffect(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("create dry-run returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("dry_run status"), Status, FString(TEXT("no_change_required")));

	const TArray<TSharedPtr<FJsonValue>>* NotesArr = nullptr;
	TestTrue(TEXT("capability_notes present"), Root->TryGetArrayField(TEXT("capability_notes"), NotesArr));

	const TSharedPtr<FJsonObject>* MetricsObj = nullptr;
	TestTrue(TEXT("metrics present"), Root->TryGetObjectField(TEXT("metrics"), MetricsObj) && MetricsObj && MetricsObj->IsValid());
	const TSharedPtr<FJsonObject>* TimingObj = nullptr;
	if (MetricsObj && MetricsObj->IsValid()
		&& (*MetricsObj)->TryGetObjectField(TEXT("timing_ms"), TimingObj) && TimingObj && TimingObj->IsValid())
	{
		double ServerTotalMs = -1.0;
		TestTrue(
			TEXT("dry_run server_total timing present"),
			(*TimingObj)->TryGetNumberField(TEXT("server_total"), ServerTotalMs));
		TestTrue(TEXT("dry_run server_total non-negative"), ServerTotalMs >= 0.0);
		TestFalse(
			TEXT("dry_run skips asset_creation timing"),
			(*TimingObj)->HasTypedField<EJson::Number>(TEXT("asset_creation")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreateReplaceDryRunTest,
	"UEREMCP.Niagara.Create.ReplaceDryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateReplaceDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-create-replace-dry","action":"create_niagara_effect","mode":"replace","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_RoundTripProbe"},"specification":{"effect_type":"projectile","element":"fire","components":["sparks"],"parameters":{"scale":1.0,"intensity":4.0}},"options":{"dry_run":true}})");

	const FString Json = UUeremcpNiagaraToolset::CreateNiagaraEffect(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("replace dry-run returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("dry_run status"), Status, FString(TEXT("no_change_required")));

	FString Summary;
	Root->TryGetStringField(TEXT("summary"), Summary);
	TestTrue(TEXT("summary mentions replace"), Summary.Contains(TEXT("replace")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBParticleRuntimeTest,
	"UEREMCP.Niagara.Create.PocBParticlesSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBParticleRuntimeTest::RunTest(const FString& Parameters)
{
	static constexpr TCHAR FireballPath[] =
		TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball.NS_POCB_Fireball");

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, FireballPath);
	if (!TestNotNull(TEXT("POC B fireball exists"), System)
		|| !TestNotNull(TEXT("editor exists"), GEditor))
	{
		return false;
	}
	// A saved system can enqueue on-demand work during load. Runtime proof must observe
	// the compiled system, not an activation deferred on compilation.
	// [VERIFIED: NiagaraSystem.h:448-455]
	System->WaitForCompilationComplete(
		/*bIncludingGPUShaders=*/false,
		/*bShowProgress=*/false);

	FNiagaraExternalEditContext EditContext(System);
	FNiagaraExt_SystemSummary Summary;
	UNiagaraExternalEditUtilities::GetSystemSummary(System, Summary, EditContext);
	TestFalse(TEXT("system summary has no edit errors"), EditContext.HasErrors());
	FNiagaraExt_SystemData SystemData;
	UNiagaraExternalEditUtilities::GetSystemData(System, SystemData, EditContext);
	AddInfo(FString::Printf(TEXT("UEREMCP_NIAGARA_SYSTEM_DATA=%s"), *SystemData.PropertyValues));
	TestTrue(
		TEXT("system state fast path is disabled"),
		SystemData.PropertyValues.Contains(TEXT("\"bAllowSystemStateFastPath\":false")));
	if (UScriptStruct* StateStruct = FindObject<UScriptStruct>(
		nullptr,
		TEXT("/Script/Niagara.NiagaraSystemStateData")))
	{
		FString StateJson;
		if (FJsonObjectConverter::UStructToJsonObjectString(
			StateStruct,
			&System->GetSystemStateData(),
			StateJson))
		{
			AddInfo(FString::Printf(TEXT("UEREMCP_NIAGARA_SYSTEM_STATE=%s"), *StateJson));
		}
	}

	int32 SpawnModuleCount = 0;
	int32 InitializeModuleCount = 0;
	int32 EnabledEmitterCount = 0;
	for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
	{
		FNiagaraExt_EmitterTopology Topology;
		const FNiagaraExt_StackItemReference EmitterRef(System, Emitter.EmitterName);
		UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, Topology, EditContext);
		EnabledEmitterCount += Topology.bEnabled ? 1 : 0;
		TArray<FString> EmitterUpdateModuleNames;
		TArray<FString> ParticleSpawnModuleNames;

		for (const FNiagaraExt_ModuleTopology& Module : Topology.EmitterUpdateScript.Modules)
		{
			EmitterUpdateModuleNames.Add(FString::Printf(
				TEXT("%s:%s"),
				*Module.ModuleName.ToString(),
				Module.Enabled ? TEXT("enabled") : TEXT("disabled")));
			if (Module.ModuleName.ToString().Contains(TEXT("Spawn")))
			{
				++SpawnModuleCount;
			}
		}
		for (const FNiagaraExt_ModuleTopology& Module : Topology.ParticleSpawnScript.Modules)
		{
			ParticleSpawnModuleNames.Add(FString::Printf(
				TEXT("%s:%s"),
				*Module.ModuleName.ToString(),
				Module.Enabled ? TEXT("enabled") : TEXT("disabled")));
			if (Module.ModuleName.ToString().Contains(TEXT("Initialize")))
			{
				++InitializeModuleCount;
			}
		}
		TArray<FNiagaraExt_ModuleInputValues> ModuleInputValues;
		UNiagaraExternalEditUtilities::GetEmitterInputValues(
			EmitterRef,
			ModuleInputValues,
			EditContext);
		for (const FNiagaraExt_ModuleInputValues& ModuleValues : ModuleInputValues)
		{
			if (ModuleValues.ModuleName != TEXT("EmitterState")
				&& ModuleValues.ModuleName != TEXT("SpawnRate")
				&& ModuleValues.ModuleName != TEXT("SpawnBurst_Instantaneous")
				&& ModuleValues.ModuleName != TEXT("InitializeParticle"))
			{
				continue;
			}
			for (const FNiagaraExt_StackInputValueEntry& Input : ModuleValues.Inputs)
			{
				const bool bRelevantInput =
					(ModuleValues.ModuleName == TEXT("EmitterState")
						&& (Input.Name == TEXT("Life Cycle Mode")
							|| Input.Name == TEXT("Loop Behavior")
							|| Input.Name == TEXT("Loop Duration")
							|| Input.Name == TEXT("Enable Distance Culling")
							|| Input.Name == TEXT("Enable Visibility Culling")))
					|| (ModuleValues.ModuleName == TEXT("SpawnRate")
						&& Input.Name == TEXT("SpawnRate"))
					|| (ModuleValues.ModuleName == TEXT("SpawnBurst_Instantaneous")
						&& Input.Name == TEXT("Spawn Count"))
					|| (ModuleValues.ModuleName == TEXT("InitializeParticle")
						&& (Input.Name == TEXT("Lifetime Min")
							|| Input.Name == TEXT("Lifetime Max")));
				if (!bRelevantInput)
				{
					continue;
				}

				FString ValueString = TEXT("other");
				if (const FNiagaraFloat* FloatValue = Input.Value.GetPtr<FNiagaraFloat>())
				{
					ValueString = FString::SanitizeFloat(FloatValue->Value);
				}
				else if (const FNiagaraInt32* IntValue = Input.Value.GetPtr<FNiagaraInt32>())
				{
					ValueString = FString::FromInt(IntValue->Value);
				}
				else if (const FNiagaraBool* BoolValue = Input.Value.GetPtr<FNiagaraBool>())
				{
					ValueString = BoolValue->GetValue() ? TEXT("true") : TEXT("false");
				}
				else if (const FNiagaraExt_StackInputData_Enum* EnumValue =
					Input.Value.GetPtr<FNiagaraExt_StackInputData_Enum>())
				{
					ValueString = EnumValue->EnumName.ToString();
				}
				AddInfo(FString::Printf(
					TEXT("UEREMCP_NIAGARA_INPUT emitter=%s module=%s input=%s value=%s"),
					*Emitter.EmitterName.ToString(),
					*ModuleValues.ModuleName.ToString(),
					*Input.Name.ToString(),
					*ValueString));
			}
		}
		AddInfo(FString::Printf(
			TEXT("UEREMCP_NIAGARA_EMITTER_TOPOLOGY name=%s enabled=%s emitter_update=[%s] particle_spawn=[%s]"),
			*Emitter.EmitterName.ToString(),
			Topology.bEnabled ? TEXT("true") : TEXT("false"),
			*FString::Join(EmitterUpdateModuleNames, TEXT(",")),
			*FString::Join(ParticleSpawnModuleNames, TEXT(","))));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestNotNull(TEXT("editor world exists"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	ANiagaraActor* Actor = World->SpawnActor<ANiagaraActor>(
		ANiagaraActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	UNiagaraComponent* Component = Actor ? Actor->GetNiagaraComponent() : nullptr;
	if (!TestNotNull(TEXT("Niagara component spawned"), Component))
	{
		if (Actor)
		{
			Actor->Destroy();
		}
		return false;
	}

	// The actor's component is already registered, so SetAutoActivate would only warn and
	// leave bAutoActivate unchanged. Disable it directly before SetAsset so SetAsset does
	// not activate an intermediate instance. [VERIFIED: ActorComponent.cpp:2856-2865]
	Component->bAutoActivate = false;
	Component->DeactivateImmediate();
	Component->SetForceSolo(true);
	Component->SetAutoDestroy(false);
	Component->SetAsset(System);
	Component->Activate(/*bReset=*/true);
	int32 LiveParticles = 0;
	int32 InitialSpawnedParticles = INDEX_NONE;
	int32 TotalSpawnedParticles = 0;
	for (int32 TickIndex = 0; TickIndex < 180; ++TickIndex)
	{
		if (TickIndex == 0)
		{
			// The first world tick moves a pending-spawn Niagara instance into simulation.
			// [VERIFIED: NiagaraSystemSimulation.cpp:1425-1502]
			World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		}
		else
		{
			// Once initialized, explicit advancement synchronously manual-ticks a solo
			// instance. [VERIFIED: NiagaraSystemInstance.cpp:987-1009]
			Component->AdvanceSimulation(1, 1.0f / 60.0f);
		}
		const FNiagaraSystemInstanceControllerPtr Controller =
			Component->GetSystemInstanceController();
		if (Controller.IsValid())
		{
			Controller->WaitForConcurrentTickAndFinalize();
			if (FNiagaraSystemInstance* Instance = Controller->GetSoloSystemInstance())
			{
				int32 TickLiveParticles = 0;
				int32 TickTotalSpawnedParticles = 0;
				for (const FNiagaraEmitterInstanceRef& EmitterInstance : Instance->GetEmitters())
				{
					TickLiveParticles += EmitterInstance->GetNumParticles();
					TickTotalSpawnedParticles += EmitterInstance->GetTotalSpawnedParticles();
				}
				if (InitialSpawnedParticles == INDEX_NONE)
				{
					InitialSpawnedParticles = TickTotalSpawnedParticles;
				}
				LiveParticles = FMath::Max(LiveParticles, TickLiveParticles);
				TotalSpawnedParticles = FMath::Max(
					TotalSpawnedParticles,
					TickTotalSpawnedParticles);
			}
			if (Controller->IsComplete())
			{
				break;
			}
		}
	}
	const FNiagaraSystemInstanceControllerPtr FinalController =
		Component->GetSystemInstanceController();
	const bool bComponentComplete =
		!FinalController.IsValid() || FinalController->IsComplete();

	AddInfo(FString::Printf(
		TEXT("UEREMCP_NIAGARA_RUNTIME_EVIDENCE={\"emitters\":%d,\"enabled_emitters\":%d,\"spawn_modules\":%d,")
		TEXT("\"initialize_modules\":%d,\"live_particles\":%d,\"total_spawned_particles\":%d,")
		TEXT("\"component_complete\":%s}"),
		Summary.Emitters.Num(),
		EnabledEmitterCount,
		SpawnModuleCount,
		InitializeModuleCount,
		LiveParticles,
		TotalSpawnedParticles,
		bComponentComplete ? TEXT("true") : TEXT("false")));

	Actor->Destroy();

	TestTrue(TEXT("at least one emitter has a spawn module"), SpawnModuleCount > 0);
	TestTrue(TEXT("at least one emitter initializes particles"), InitializeModuleCount > 0);
	TestTrue(TEXT("system spawned particles"), TotalSpawnedParticles > 0);
	TestTrue(TEXT("system has live particles during simulation"), LiveParticles > 0);
	TestTrue(
		TEXT("looping emitters continue spawning after activation"),
		InitialSpawnedParticles >= 0 && TotalSpawnedParticles > InitialSpawnedParticles);
	TestFalse(TEXT("looping system remains active"), bComponentComplete);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
