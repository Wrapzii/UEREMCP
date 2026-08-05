// Editor automation tests for submit_niagara_graph (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraSubmit.h"
#include "UeremcpNiagaraToolset.h"
#include "UeremcpEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraSubmitPathGuardTest,
	"UEREMCP.Niagara.Submit.PathGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraSubmitPathGuardTest::RunTest(const FString& Parameters)
{
	const FString RejectJson = UUeremcpNiagaraToolset::SubmitNiagaraGraph(TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-submit-offroot","action":"submit_niagara_graph","target":{"asset_path":"/Game/VFX/NS_Fireball"},"options":{"dry_run":true},"specification":{"graphs":[{"graph_type":"NiagaraSystemGraph"}]}})"));
	TSharedPtr<FJsonObject> RejectRoot;
	const TSharedRef<TJsonReader<>> RejectReader = TJsonReaderFactory<>::Create(RejectJson);
	TestTrue(TEXT("off-root submit JSON"), FJsonSerializer::Deserialize(RejectReader, RejectRoot) && RejectRoot.IsValid());
	if (RejectRoot.IsValid())
	{
		TestEqual(TEXT("rejected"), RejectRoot->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraSubmitSpecRequiresGraphsTest,
	"UEREMCP.Niagara.Submit.SpecRequiresGraphs",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraSubmitSpecRequiresGraphsTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraSubmitSpec Spec;
	FString SpecError;
	TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
	TestFalse(TEXT("empty spec fails"), FUeremcpNiagaraSubmit::ParseSpecification(Empty, Spec, SpecError));
	TestTrue(
		TEXT("error mentions graphs or emitters"),
		SpecError.Contains(TEXT("graphs")) || SpecError.Contains(TEXT("emitters")));

	TSharedPtr<FJsonObject> EmittersOnly = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Emitters;
	TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
	E->SetStringField(TEXT("name"), TEXT("CustomSpark"));
	TArray<TSharedPtr<FJsonValue>> Mods;
	TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
	M->SetStringField(TEXT("primitive_id"), TEXT("spawn_rate"));
	TSharedPtr<FJsonObject> Inputs = MakeShared<FJsonObject>();
	Inputs->SetNumberField(TEXT("SpawnRate"), 8);
	M->SetObjectField(TEXT("inputs"), Inputs);
	Mods.Add(MakeShared<FJsonValueObject>(M));
	E->SetArrayField(TEXT("modules"), Mods);
	Emitters.Add(MakeShared<FJsonValueObject>(E));
	EmittersOnly->SetArrayField(TEXT("emitters"), Emitters);

	FUeremcpNiagaraSubmitSpec Authored;
	FString AuthError;
	TestTrue(
		TEXT("emitters[] alone parses"),
		FUeremcpNiagaraSubmit::ParseSpecification(EmittersOnly, Authored, AuthError));
	TestTrue(TEXT("synthesized graphs"), Authored.Graphs.Num() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraSubmitSimTargetLifeCycleParseTest,
	"UEREMCP.Niagara.Submit.SimTargetLifeCycleParse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraSubmitSimTargetLifeCycleParseTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> SpecObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Emitters;
	TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
	E->SetStringField(TEXT("name"), TEXT("Sparks1"));
	E->SetStringField(TEXT("sim_target"), TEXT("GPU"));
	TSharedPtr<FJsonObject> LC = MakeShared<FJsonObject>();
	LC->SetNumberField(TEXT("loop_duration"), 2.5);
	LC->SetStringField(TEXT("loop_behavior"), TEXT("Infinite"));
	E->SetObjectField(TEXT("life_cycle"), LC);
	TArray<TSharedPtr<FJsonValue>> Mods;
	TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
	M->SetStringField(TEXT("primitive_id"), TEXT("emitter_state"));
	Mods.Add(MakeShared<FJsonValueObject>(M));
	E->SetArrayField(TEXT("modules"), Mods);
	Emitters.Add(MakeShared<FJsonValueObject>(E));
	SpecObj->SetArrayField(TEXT("emitters"), Emitters);

	FUeremcpNiagaraSubmitSpec Spec;
	FString SpecError;
	TestTrue(TEXT("parse"), FUeremcpNiagaraSubmit::ParseSpecification(SpecObj, Spec, SpecError));
	TestTrue(TEXT("has graphs"), Spec.Graphs.Num() >= 1);
	TestTrue(TEXT("emitter_properties apply default on"), Spec.bApplyEmitterProperties);

	bool bFoundSim = false;
	for (const TSharedPtr<FJsonObject>& G : Spec.Graphs)
	{
		if (!G.IsValid())
		{
			continue;
		}
		FString GraphType;
		G->TryGetStringField(TEXT("graph_type"), GraphType);
		if (GraphType != TEXT("NiagaraEmitterGraph"))
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* Ext = nullptr;
		const TSharedPtr<FJsonObject>* Niagara = nullptr;
		if (G->TryGetObjectField(TEXT("extensions"), Ext) && Ext && (*Ext)->TryGetObjectField(TEXT("niagara"), Niagara))
		{
			FString Sim;
			(*Niagara)->TryGetStringField(TEXT("sim_target"), Sim);
			if (Sim == TEXT("GPU") || Sim == TEXT("GPUComputeSim"))
			{
				bFoundSim = true;
			}
		}
	}
	TestTrue(TEXT("sim_target propagated into synthesized emitter graph"), bFoundSim);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraSubmitDryRunMagecraftTest,
	"UEREMCP.Niagara.Submit.DryRunMagecraft",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraSubmitDryRunMagecraftTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Magecraft mutate allowed"),
		UeremcpNiagaraPaths::IsAllowedMutatePath(
			TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast")));

	const FString Json = UUeremcpNiagaraToolset::SubmitNiagaraGraph(TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-submit-dry","action":"submit_niagara_graph","mode":"replace","target":{"asset_path":"/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"},"options":{"dry_run":true},"specification":{"graphs":[{"graph_type":"NiagaraSystemGraph","graph_id":"/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast::System","variables":[],"fidelity":{"round_trip_supported":false}}]}})"));

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("submit dry_run JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	// Existing Magecraft asset → dry_run succeeds as no_change_required; missing → rejected.
	TestTrue(
		TEXT("dry_run status ok or rejected if asset missing"),
		Status == TEXT("no_change_required") || Status == TEXT("rejected") || Status == TEXT("failed_validation"));

	TestFalse(TEXT("never claims modified_and_validated"), Status == TEXT("modified_and_validated"));
	TestFalse(TEXT("never claims created_and_validated"), Status == TEXT("created_and_validated"));

	if (Status == TEXT("no_change_required"))
	{
		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (Root->TryGetObjectField(TEXT("result"), Result) && Result && (*Result).IsValid())
		{
			const TSharedPtr<FJsonObject>* Fidelity = nullptr;
			if ((*Result)->TryGetObjectField(TEXT("fidelity"), Fidelity) && Fidelity && (*Fidelity).IsValid())
			{
				TestFalse(
					TEXT("round_trip_supported false"),
					(*Fidelity)->GetBoolField(TEXT("round_trip_supported")));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraSubmitParseModuleStackTest,
	"UEREMCP.Niagara.Submit.ParseModuleStack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraSubmitParseModuleStackTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> SpecObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Graphs;

	TSharedPtr<FJsonObject> Stack = MakeShared<FJsonObject>();
	Stack->SetStringField(TEXT("graph_type"), TEXT("NiagaraModuleStack"));
	TSharedPtr<FJsonObject> ExtRoot = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Niagara = MakeShared<FJsonObject>();
	Niagara->SetStringField(TEXT("emitter_name"), TEXT("Main"));
	Niagara->SetStringField(TEXT("script_usage"), TEXT("ParticleUpdateScript"));
	ExtRoot->SetObjectField(TEXT("niagara"), Niagara);
	Stack->SetObjectField(TEXT("extensions"), ExtRoot);

	TArray<TSharedPtr<FJsonValue>> Nodes;
	TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
	Node->SetStringField(TEXT("title"), TEXT("ParticleState"));
	Node->SetStringField(TEXT("semantic_type"), TEXT("niagara_module"));
	Node->SetBoolField(TEXT("enabled"), false);
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetStringField(TEXT("module_script"), TEXT("/Niagara/Modules/Update/ParticleState.ParticleState"));
	Node->SetObjectField(TEXT("properties"), Props);
	Nodes.Add(MakeShared<FJsonValueObject>(Node));
	Stack->SetArrayField(TEXT("nodes"), Nodes);
	Graphs.Add(MakeShared<FJsonValueObject>(Stack));
	SpecObj->SetArrayField(TEXT("graphs"), Graphs);

	FUeremcpNiagaraSubmitSpec Spec;
	FString SpecError;
	TestTrue(TEXT("parse ok"), FUeremcpNiagaraSubmit::ParseSpecification(SpecObj, Spec, SpecError));
	TestEqual(TEXT("one graph"), Spec.Graphs.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
