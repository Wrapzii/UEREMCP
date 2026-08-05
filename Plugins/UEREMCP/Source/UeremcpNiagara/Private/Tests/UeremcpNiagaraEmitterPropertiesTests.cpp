// Editor automation tests for Niagara emitter properties + stack inputs + hash round-trip (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraEmitterProperties.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraStackInputs.h"
#include "UeremcpEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraSimTargetNormalizeTest,
	"UEREMCP.Niagara.EmitterProperties.NormalizeSimTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraSimTargetNormalizeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("CPU"), FUeremcpNiagaraEmitterProperties::NormalizeSimTarget(TEXT("CPU")), FString(TEXT("CPUSim")));
	TestEqual(TEXT("GPU"), FUeremcpNiagaraEmitterProperties::NormalizeSimTarget(TEXT("GPU")), FString(TEXT("GPUComputeSim")));
	TestEqual(
		TEXT("GPUComputeSim"),
		FUeremcpNiagaraEmitterProperties::NormalizeSimTarget(TEXT("GPUComputeSim")),
		FString(TEXT("GPUComputeSim")));
	TestTrue(TEXT("bad empty"), FUeremcpNiagaraEmitterProperties::NormalizeSimTarget(TEXT("bogus")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreateParseSimTargetLifeCycleTest,
	"UEREMCP.Niagara.Create.ParseSimTargetLifeCycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateParseSimTargetLifeCycleTest::RunTest(const FString& Parameters)
{
	const FString RequestJson = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-sim","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpTests/NS_SimLC"},"specification":{"effect_type":"custom","emitters":[{"name":"Sparks1","sim_target":"GPU","loop_duration":1.5,"life_cycle":{"loop_behavior":"Infinite"},"modules":[{"primitive_id":"emitter_state"},{"primitive_id":"spawn_rate","inputs":{"SpawnRate":{"mode":"linked","linked_variable":"User.Intensity"}}}]}]}})");

	FUeremcpRequest Request;
	FString ParseError;
	TestTrue(TEXT("envelope parses"), FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError));

	FUeremcpNiagaraCreateSpec Spec;
	FString SpecError;
	TestTrue(TEXT("spec parses"), FUeremcpNiagaraCreate::ParseSpecification(Request, Spec, SpecError));
	TestEqual(TEXT("one emitter"), Spec.Emitters.Num(), 1);
	if (Spec.Emitters.Num() > 0)
	{
		TestEqual(TEXT("sim_target GPU→GPUComputeSim"), Spec.Emitters[0].SimTarget, FString(TEXT("GPUComputeSim")));
		TestTrue(TEXT("loop_duration set"), Spec.Emitters[0].LoopDuration.IsSet());
		if (Spec.Emitters[0].LoopDuration.IsSet())
		{
			TestEqual(TEXT("loop_duration 1.5"), Spec.Emitters[0].LoopDuration.GetValue(), 1.5f);
		}
		TestEqual(TEXT("loop_behavior"), Spec.Emitters[0].LoopBehavior, FString(TEXT("Infinite")));
		TestTrue(TEXT("has modules"), Spec.Emitters[0].Modules.Num() >= 2);
		if (Spec.Emitters[0].Modules.Num() >= 2 && Spec.Emitters[0].Modules[1].Inputs.IsValid())
		{
			// UE 5.8: TryGetField(FieldName) returns TSharedPtr (not out-param).
			// [VERIFIED: Engine/.../JsonObject.h:354]
			const TSharedPtr<FJsonValue> Spawn =
				Spec.Emitters[0].Modules[1].Inputs->TryGetField(TEXT("SpawnRate"));
			TestTrue(
				TEXT("SpawnRate linked object"),
				Spawn.IsValid() && Spawn->Type == EJson::Object);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraStackInputLinkedBuildTest,
	"UEREMCP.Niagara.StackInputs.BuildLinked",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraStackInputLinkedBuildTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("mode"), TEXT("linked"));
	Obj->SetStringField(TEXT("linked_variable"), TEXT("User.Intensity"));
	TSharedPtr<FJsonValue> JsonVal = MakeShared<FJsonValueObject>(Obj);

	FNiagaraExt_StackInputValue Existing;
	FNiagaraExt_StackInputValue Out;
	FString Skip;
	TestTrue(
		TEXT("linked builds"),
		FUeremcpNiagaraStackInputs::TryBuildStackInputValue(Existing, JsonVal, Out, Skip));
	const FNiagaraExt_StackInputData_Linked* Linked = Out.GetPtr<FNiagaraExt_StackInputData_Linked>();
	TestTrue(TEXT("is linked struct"), Linked != nullptr);
	if (Linked)
	{
		TestEqual(TEXT("linked name"), Linked->LinkedVariable.Name.ToString(), FString(TEXT("User.Intensity")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraHashRoundTripRetrieveSubmitTest,
	"UEREMCP.Niagara.HashRoundTrip.RetrieveSubmitRetrieve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraHashRoundTripRetrieveSubmitTest::RunTest(const FString& Parameters)
{
	auto MakeGraph = [](const FString& GraphId, const FString& Hash) -> TSharedPtr<FJsonValue>
	{
		TSharedPtr<FJsonObject> G = MakeShared<FJsonObject>();
		G->SetStringField(TEXT("graph_type"), TEXT("NiagaraModuleStack"));
		G->SetStringField(TEXT("graph_id"), GraphId);
		G->SetStringField(TEXT("content_hash"), Hash);
		G->SetStringField(TEXT("revision"), Hash);
		TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
		Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
		G->SetObjectField(TEXT("fidelity"), Fidelity);
		return MakeShared<FJsonValueObject>(G);
	};

	TArray<TSharedPtr<FJsonValue>> Pre;
	Pre.Add(MakeGraph(TEXT("g1"), TEXT("sha256:aaa")));
	Pre.Add(MakeGraph(TEXT("g2"), TEXT("sha256:bbb")));

	TArray<TSharedPtr<FJsonValue>> PostMatch = Pre;
	FUeremcpNiagaraHashRoundTripResult Ok;
	TestTrue(
		TEXT("eval ok"),
		FUeremcpNiagaraHashRoundTrip::EvaluateRetrieveSubmitRetrieveStability(Pre, PostMatch, Ok));
	TestTrue(TEXT("proven stable"), Ok.bRetrieveSubmitRetrieveStable);
	TestTrue(TEXT("round_trip_supported true when proven"), Ok.bRoundTripSupported);

	TArray<TSharedPtr<FJsonValue>> PostDrift;
	PostDrift.Add(MakeGraph(TEXT("g1"), TEXT("sha256:aaa")));
	PostDrift.Add(MakeGraph(TEXT("g2"), TEXT("sha256:CHANGED")));
	FUeremcpNiagaraHashRoundTripResult Drift;
	FUeremcpNiagaraHashRoundTrip::EvaluateRetrieveSubmitRetrieveStability(Pre, PostDrift, Drift);
	TestFalse(TEXT("drift not stable"), Drift.bRetrieveSubmitRetrieveStable);
	TestFalse(TEXT("round_trip stays false on drift"), Drift.bRoundTripSupported);
	TestTrue(TEXT("failure_mode set"), !Drift.FailureMode.IsEmpty());

	TSharedPtr<FJsonObject> Diag = FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(Drift);
	TestFalse(TEXT("diag round_trip false"), Diag->GetBoolField(TEXT("round_trip_supported")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
