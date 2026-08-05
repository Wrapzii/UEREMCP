// Editor automation tests for UeremcpNiagara create (WS-07).

#include "CoreMinimal.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
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
	FUeremcpNiagaraCreateCustomModuleStackParseTest,
	"UEREMCP.Niagara.Create.CustomModuleStackParse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateCustomModuleStackParseTest::RunTest(const FString& Parameters)
{
	const FString RequestJson = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-custom-parse","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpPoc/Magecraft/NS_UeremcpCustomTripleStack"},"specification":{"name":"NS_UeremcpCustomTripleStack","effect_type":"custom","element":"arcane","emitters":[{"name":"GroundMist","renderer":{"type":"sprite"},"modules":[{"primitive_id":"spawn_rate","script":"EmitterUpdateScript","inputs":{"SpawnRate":40}},{"primitive_id":"initialize_particle"},{"primitive_id":"update_age"}]},{"name":"RisingWisps","modules":[{"primitive_id":"spawn_rate"},{"primitive_id":"add_velocity"}]},{"name":"ImpactFlash","modules":[{"asset_path":"/Niagara/Modules/Emitter/SpawnBurst_Instantaneous","script":"EmitterUpdateScript"},{"primitive_id":"color"}]}],"parameters":{"intensity":5.0}},"options":{"dry_run":true}})");
	FUeremcpRequest Request;
	FString Error;
	TestTrue(TEXT("custom envelope parses"), FUeremcpEnvelope::ParseRequest(RequestJson, Request, Error));

	FUeremcpNiagaraCreateSpec Spec;
	TestTrue(TEXT("custom specification parses"), FUeremcpNiagaraCreate::ParseSpecification(Request, Spec, Error));
	TestEqual(TEXT("three LLM-named emitters"), Spec.Emitters.Num(), 3);
	TestEqual(TEXT("GroundMist"), Spec.Emitters[0].Name, FString(TEXT("GroundMist")));
	TestEqual(TEXT("RisingWisps"), Spec.Emitters[1].Name, FString(TEXT("RisingWisps")));
	TestEqual(TEXT("ImpactFlash"), Spec.Emitters[2].Name, FString(TEXT("ImpactFlash")));
	TestTrue(TEXT("no ice_creep role"), Spec.Emitters[0].Role.IsEmpty());
	TestTrue(TEXT("custom stack flag"), Spec.Emitters[0].bCustomModuleStack);
	TestEqual(
		TEXT("Minimal substrate"),
		Spec.Emitters[0].TemplatePath,
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal")));
	TestTrue(TEXT("GroundMist has modules"), Spec.Emitters[0].Modules.Num() >= 3);
	TestEqual(
		TEXT("spawn_rate resolved"),
		Spec.Emitters[0].Modules[0].AssetPath,
		FString(TEXT("/Niagara/Modules/Emitter/SpawnRate")));
	TestTrue(TEXT("SpawnRate inputs present"), Spec.Emitters[0].Modules[0].Inputs.IsValid());
	TestEqual(TEXT("sprite renderer hint"), Spec.Emitters[0].RendererType, FString(TEXT("sprite")));

	const FString DryJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(RequestJson);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DryJson);
	TestTrue(TEXT("dry_run JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (Root.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
		TestTrue(TEXT("capability_notes"), Root->TryGetArrayField(TEXT("capability_notes"), Notes));
		bool bMentionsPrimary = false;
		if (Notes)
		{
			for (const TSharedPtr<FJsonValue>& N : *Notes)
			{
				if (N->AsString().Contains(TEXT("PRIMARY PATH")))
				{
					bMentionsPrimary = true;
					break;
				}
			}
		}
		TestTrue(TEXT("notes say PRIMARY PATH modules[]"), bMentionsPrimary);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreateIceEmittersParseTest,
	"UEREMCP.Niagara.Create.IceMultiEmitterParse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateIceEmittersParseTest::RunTest(const FString& Parameters)
{
	const FString RequestJson = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-ice-parse","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpPoc/Magecraft/NS_UeremcpIceFreezeDome"},"specification":{"effect_type":"ice","element":"ice","emitters":[{"role":"ice_creep","name":"IceCreep"},{"role":"freeze_dome","name":"FreezeDome"},{"role":"sparks","name":"FrostSparks"}],"parameters":{"primary_color":[0.55,0.85,1.0,1.0],"intensity":6.0}},"options":{"dry_run":true}})");
	FUeremcpRequest Request;
	FString Error;
	TestTrue(TEXT("ice envelope parses"), FUeremcpEnvelope::ParseRequest(RequestJson, Request, Error));

	FUeremcpNiagaraCreateSpec Spec;
	TestTrue(TEXT("ice specification parses"), FUeremcpNiagaraCreate::ParseSpecification(Request, Spec, Error));
	TestEqual(TEXT("three emitter plans"), Spec.Emitters.Num(), 3);
	TestEqual(TEXT("first name IceCreep"), Spec.Emitters[0].Name, FString(TEXT("IceCreep")));
	TestEqual(
		TEXT("ice_creep template"),
		Spec.Emitters[0].TemplatePath,
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles")));
	TestEqual(
		TEXT("freeze_dome template"),
		Spec.Emitters[1].TemplatePath,
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates")));
	TestEqual(
		TEXT("sparks template"),
		Spec.Emitters[2].TemplatePath,
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst")));

	// Omitting emitters/components still defaults for effect_type=ice.
	const FString DefaultJson = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-ice-default","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpPoc/Magecraft/NS_UeremcpIceDefault"},"specification":{"effect_type":"freeze","element":"ice"},"options":{"dry_run":true}})");
	FUeremcpRequest DefaultRequest;
	TestTrue(
		TEXT("default ice envelope parses"),
		FUeremcpEnvelope::ParseRequest(DefaultJson, DefaultRequest, Error));
	FUeremcpNiagaraCreateSpec DefaultSpec;
	TestTrue(
		TEXT("default ice specification parses"),
		FUeremcpNiagaraCreate::ParseSpecification(DefaultRequest, DefaultSpec, Error));
	TestEqual(TEXT("default ice three emitters"), DefaultSpec.Emitters.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocCVariationParseTest,
	"UEREMCP.Niagara.Create.PocCVariationParse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocCVariationParseTest::RunTest(const FString& Parameters)
{
	const FString RequestJson = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-poc-c-parse","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpPoc/NS_POCC_Ice"},"specification":{"effect_type":"projectile","element":"ice","base_system":{"asset_path":"/Game/__UeremcpPoc/NS_POCB_Fireball"},"components":[{"role":"crystalline","archetype":"niagara.emitter.sparks.v1"},{"role":"ice_impact","archetype":"niagara.emitter.impact_burst.v1"}],"parameters":{"primary_color":[0.6,0.85,1.0,1.0],"intensity":6.0}},"options":{"dry_run":true}})");
	FUeremcpRequest Request;
	FString Error;
	TestTrue(TEXT("variation envelope parses"), FUeremcpEnvelope::ParseRequest(RequestJson, Request, Error));

	FUeremcpNiagaraCreateSpec Spec;
	TestTrue(TEXT("variation specification parses"), FUeremcpNiagaraCreate::ParseSpecification(Request, Spec, Error));
	TestEqual(
		TEXT("base system retained"),
		Spec.BaseSystemPath,
		FString(TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball")));
	TestEqual(TEXT("two additive variation roles"), Spec.ComponentRoles.Num(), 2);
	TestTrue(TEXT("crystalline requested"), Spec.ComponentRoles.Contains(TEXT("crystalline")));
	TestTrue(TEXT("changed impact requested"), Spec.ComponentRoles.Contains(TEXT("ice_impact")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocCVariationRuntimeTest,
	"UEREMCP.Niagara.Create.PocCVariationRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocCVariationRuntimeTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball");
	const FString TargetPath = TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariationDirect");
	UNiagaraSystem* Source = Cast<UNiagaraSystem>(FSoftObjectPath(SourcePath).TryLoad());
	if (!TestNotNull(TEXT("POC B source fireball exists"), Source))
	{
		return false;
	}

	const FString Request = FString::Printf(
		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"ws07-poc-c-runtime\",\"action\":\"create_niagara_effect\",")
		TEXT("\"mode\":\"replace\",\"target\":{\"asset_path\":\"%s\"},")
		TEXT("\"specification\":{\"name\":\"NS_POCC_IceVariationDirect\",\"effect_type\":\"projectile\",\"element\":\"ice\",")
		TEXT("\"base_system\":{\"asset_path\":\"%s\"},\"components\":[\"crystalline\",\"ice_impact\"],")
		TEXT("\"parameters\":{\"primary_color\":[0.6,0.85,1.0,1.0],\"secondary_color\":[0.9,0.95,1.0,1.0],\"scale\":1.0,\"intensity\":6.0}},")
		TEXT("\"options\":{\"dry_run\":false,\"allow_destructive\":true,\"compile\":true,\"validate\":true,\"save\":true,\"timeout_ms\":0}}"),
		*TargetPath,
		*SourcePath);
	const FString ResponseJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(Request);
	TSharedPtr<FJsonObject> Response;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
	if (!TestTrue(
		TEXT("variation response parses"),
		FJsonSerializer::Deserialize(Reader, Response) && Response.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	double RoundTrips = 0.0;
	TestTrue(
		TEXT("variation is one MCP round trip"),
		Response->TryGetObjectField(TEXT("metrics"), Metrics)
			&& Metrics
			&& (*Metrics)->TryGetNumberField(TEXT("mcp_round_trips"), RoundTrips)
			&& RoundTrips == 1.0);

	UNiagaraSystem* Target = Cast<UNiagaraSystem>(FSoftObjectPath(TargetPath).TryLoad());
	if (!TestNotNull(TEXT("ice variation exists"), Target))
	{
		return false;
	}
	FNiagaraExternalEditContext SourceContext(Source);
	FNiagaraExternalEditContext TargetContext(Target);
	FNiagaraExt_SystemSummary SourceSummary;
	FNiagaraExt_SystemSummary TargetSummary;
	UNiagaraExternalEditUtilities::GetSystemSummary(Source, SourceSummary, SourceContext);
	UNiagaraExternalEditUtilities::GetSystemSummary(Target, TargetSummary, TargetContext);
	TestFalse(TEXT("source summary has no errors"), SourceContext.HasErrors());
	TestFalse(TEXT("target summary has no errors"), TargetContext.HasErrors());

	TSet<FName> TargetEmitterNames;
	for (const FNiagaraExt_EmitterSummary& Emitter : TargetSummary.Emitters)
	{
		TargetEmitterNames.Add(Emitter.EmitterName);
	}
	for (const FNiagaraExt_EmitterSummary& Emitter : SourceSummary.Emitters)
	{
		TestTrue(
			*FString::Printf(TEXT("inherited emitter preserved: %s"), *Emitter.EmitterName.ToString()),
			TargetEmitterNames.Contains(Emitter.EmitterName));
	}
	TestTrue(TEXT("crystalline emitter added"), TargetEmitterNames.Contains(FName(TEXT("Crystalline"))));
	TestTrue(TEXT("ice impact emitter added"), TargetEmitterNames.Contains(FName(TEXT("IceImpact"))));

	FNiagaraExt_UserVariables UserVariables;
	UNiagaraExternalEditUtilities::GetUserVariables(Target, UserVariables, TargetContext);
	TSet<FName> UserVariableNames;
	for (const FNiagaraExt_UserVariable& Variable : UserVariables.UserVariables)
	{
		UserVariableNames.Add(Variable.Name);
	}
	TestTrue(TEXT("ice color parameter exists"), UserVariableNames.Contains(FName(TEXT("User.Color"))));
	TestTrue(TEXT("ice secondary color parameter exists"), UserVariableNames.Contains(FName(TEXT("User.SecondaryColor"))));
	TestTrue(TEXT("ice scale parameter exists"), UserVariableNames.Contains(FName(TEXT("User.Scale"))));
	TestTrue(TEXT("ice intensity parameter exists"), UserVariableNames.Contains(FName(TEXT("User.Intensity"))));
	return !HasAnyErrors();
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
	System->RequestCompile(/*bForce=*/true);
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
		TEXT("system state fast path is disabled for stateful emitters"),
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
	int32 ColorCapableRendererCount = 0;
	int32 ParticleColorBoundRendererCount = 0;
	bool bSystemLifecycleInfinite = false;
	TArray<FNiagaraExt_ModuleInputValues> SystemUpdateInputValues;
	UNiagaraExternalEditUtilities::GetScriptStackInputValues(
		FNiagaraExt_StackItemReference(System, NAME_None, TEXT("SystemUpdateScript")),
		SystemUpdateInputValues,
		EditContext);
	for (const FNiagaraExt_ModuleInputValues& ModuleValues : SystemUpdateInputValues)
	{
		if (ModuleValues.ModuleName != TEXT("SystemState"))
		{
			continue;
		}
		for (const FNiagaraExt_StackInputValueEntry& Input : ModuleValues.Inputs)
		{
			if (Input.Name != TEXT("Loop Behavior"))
			{
				continue;
			}
			if (const FNiagaraExt_StackInputData_Enum* EnumValue =
				Input.Value.GetPtr<FNiagaraExt_StackInputData_Enum>())
			{
				const FString DisplayName = EnumValue->DisplayName.ToString();
				AddInfo(FString::Printf(
					TEXT("UEREMCP_NIAGARA_SYSTEM_INPUT module=SystemState input=Loop Behavior value=%s display=%s"),
					*EnumValue->EnumName.ToString(),
					*DisplayName));
				bSystemLifecycleInfinite =
					DisplayName.Equals(TEXT("Infinite"), ESearchCase::IgnoreCase);
			}
		}
	}
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
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (!EmitterData)
		{
			continue;
		}
		EmitterData->ForEachRenderer([&](const UNiagaraRendererProperties* Renderer)
		{
			const FNiagaraVariableAttributeBinding* ColorBinding = nullptr;
			if (const UNiagaraSpriteRendererProperties* Sprite =
				Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				ColorBinding = &Sprite->ColorBinding;
			}
			else if (const UNiagaraMeshRendererProperties* Mesh =
				Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				ColorBinding = &Mesh->ColorBinding;
			}
			else if (const UNiagaraRibbonRendererProperties* Ribbon =
				Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				ColorBinding = &Ribbon->ColorBinding;
			}
			if (!ColorBinding)
			{
				return;
			}

			++ColorCapableRendererCount;
			const FName BindingName =
				ColorBinding->GetParamMapBindableVariable().GetName();
			if (BindingName == TEXT("Particles.Color"))
			{
				++ParticleColorBoundRendererCount;
			}
			AddInfo(FString::Printf(
				TEXT("UEREMCP_NIAGARA_RENDERER_COLOR emitter=%s renderer=%s binding=%s source_exists=%s"),
				*Handle.GetName().ToString(),
				*Renderer->GetClass()->GetName(),
				*BindingName.ToString(),
				ColorBinding->DoesBindingExistOnSource() ? TEXT("true") : TEXT("false")));
		});
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
	int32 RuntimeEmitterInstances = 0;
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
				RuntimeEmitterInstances = FMath::Max(
					RuntimeEmitterInstances,
					Instance->GetEmitters().Num());
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
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		AddInfo(FString::Printf(
			TEXT("UEREMCP_NIAGARA_EMITTER_READY name=%s ready=%s"),
			*Handle.GetName().ToString(),
			EmitterData && EmitterData->IsReadyToRun() ? TEXT("true") : TEXT("false")));
	}

	AddInfo(FString::Printf(
		TEXT("UEREMCP_NIAGARA_RUNTIME_EVIDENCE={\"emitters\":%d,\"enabled_emitters\":%d,\"spawn_modules\":%d,")
		TEXT("\"initialize_modules\":%d,\"live_particles\":%d,\"total_spawned_particles\":%d,")
		TEXT("\"runtime_emitter_instances\":%d,\"compiled_emitter_data\":%d,\"system_valid\":%s,")
		TEXT("\"system_ready_to_run\":%s,\"component_complete\":%s}"),
		Summary.Emitters.Num(),
		EnabledEmitterCount,
		SpawnModuleCount,
		InitializeModuleCount,
		LiveParticles,
		TotalSpawnedParticles,
		RuntimeEmitterInstances,
		System->GetEmitterCompiledData().Num(),
		System->IsValid() ? TEXT("true") : TEXT("false"),
		System->IsReadyToRun() ? TEXT("true") : TEXT("false"),
		bComponentComplete ? TEXT("true") : TEXT("false")));

	Actor->Destroy();

	TestTrue(TEXT("system lifecycle is explicitly infinite"), bSystemLifecycleInfinite);
	TestTrue(TEXT("at least one emitter has a spawn module"), SpawnModuleCount > 0);
	TestTrue(TEXT("at least one emitter initializes particles"), InitializeModuleCount > 0);
	TestTrue(TEXT("at least one color-capable renderer exists"), ColorCapableRendererCount > 0);
	TestEqual(
		TEXT("all color-capable renderers consume initialized Particles.Color"),
		ParticleColorBoundRendererCount,
		ColorCapableRendererCount);
	TestTrue(TEXT("system spawned particles"), TotalSpawnedParticles > 0);
	TestTrue(TEXT("system has live particles during simulation"), LiveParticles > 0);
	TestTrue(
		TEXT("looping emitters continue spawning after activation"),
		InitialSpawnedParticles >= 0 && TotalSpawnedParticles > InitialSpawnedParticles);
	TestFalse(TEXT("looping system remains active"), bComponentComplete);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
