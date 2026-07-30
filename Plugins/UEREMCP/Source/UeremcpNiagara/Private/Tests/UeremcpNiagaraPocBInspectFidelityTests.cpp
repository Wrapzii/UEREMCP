// Editor automation tests for UeremcpNiagara POC B inspect fidelity (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraPocBGates.h"
#include "UeremcpNiagaraPocBInspectFidelity.h"
#include "UeremcpNiagaraRoundTrip.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<TSharedPtr<FJsonValue>> MakeInspectGraphArrayFromFixtureLike()
	{
		TArray<TSharedPtr<FJsonValue>> Graphs;

		TSharedPtr<FJsonObject> System = MakeShared<FJsonObject>();
		System->SetStringField(TEXT("graph_type"), TEXT("NiagaraSystemGraph"));
		TSharedPtr<FJsonObject> SystemExt = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> SystemNiagara = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Dependencies = MakeShared<FJsonObject>();
		Dependencies->SetNumberField(TEXT("used_data_interfaces"), 2);
		SystemNiagara->SetObjectField(TEXT("dependencies"), Dependencies);
		TSharedPtr<FJsonObject> Compile = MakeShared<FJsonObject>();
		Compile->SetBoolField(TEXT("bHasErrors"), false);
		SystemNiagara->SetObjectField(TEXT("compile"), Compile);
		SystemExt->SetObjectField(TEXT("niagara"), SystemNiagara);
		System->SetObjectField(TEXT("extensions"), SystemExt);
		Graphs.Add(MakeShared<FJsonValueObject>(System));

		auto MakeEmitter = [](const FString& Name, bool bWithMaterialPath) {
			TSharedPtr<FJsonObject> Emitter = MakeShared<FJsonObject>();
			Emitter->SetStringField(TEXT("graph_type"), TEXT("NiagaraEmitterGraph"));
			Emitter->SetStringField(TEXT("graph_name"), Name);
			TSharedPtr<FJsonObject> Ext = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> Niagara = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Renderers;
			TSharedPtr<FJsonObject> Renderer = MakeShared<FJsonObject>();
			Renderer->SetNumberField(TEXT("renderer_index"), 0);
			if (bWithMaterialPath)
			{
				Renderer->SetStringField(TEXT("material_path"), TEXT("/Game/__UeremcpTests/Materials/MI_Sparks.MI_Sparks"));
				Renderer->SetStringField(
					TEXT("material_path_fidelity"),
					TEXT("extracted_from_property_values_not_validated"));
			}
			Renderers.Add(MakeShared<FJsonValueObject>(Renderer));
			Niagara->SetArrayField(TEXT("renderers"), Renderers);
			Ext->SetObjectField(TEXT("niagara"), Niagara);
			Emitter->SetObjectField(TEXT("extensions"), Ext);
			return Emitter;
		};

		Graphs.Add(MakeShared<FJsonValueObject>(MakeEmitter(TEXT("Sparks"), true)));
		Graphs.Add(MakeShared<FJsonValueObject>(MakeEmitter(TEXT("ImpactBurst"), false)));
		return Graphs;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBInspectFidelityOfflineTest,
	"UEREMCP.Niagara.Create.PocBInspectFidelityOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBInspectFidelityOfflineTest::RunTest(const FString& Parameters)
{
	const TArray<FString> ExpectedEmitters = {TEXT("Sparks"), TEXT("ImpactBurst")};
	const TArray<TSharedPtr<FJsonValue>> Graphs = MakeInspectGraphArrayFromFixtureLike();

	const FUeremcpNiagaraPocBInspectSignals Signals =
		FUeremcpNiagaraPocBInspectFidelity::Evaluate(ExpectedEmitters, Graphs);

	TestTrue(TEXT("signals evaluated"), Signals.bEvaluated);
	TestEqual(TEXT("emitters with renderer refs"), Signals.EmittersWithRendererRefs, 2);
	TestEqual(TEXT("total renderer refs"), Signals.TotalRendererRefs, 2);
	TestEqual(TEXT("extracted material paths"), Signals.RenderersWithExtractedMaterialPath, 1);
	TestTrue(TEXT("dependencies present"), Signals.bDependenciesPresent);
	TestEqual(TEXT("used data interfaces"), Signals.UsedDataInterfaces, 2);
	TestFalse(TEXT("compile errors"), Signals.bCompileHasErrors);
	TestEqual(TEXT("no missing emitters"), Signals.EmittersMissingRenderers.Num(), 0);

	FUeremcpNiagaraCreateResult CreateResult;
	CreateResult.EmittersAdded = ExpectedEmitters;
	CreateResult.MaterialBindings.bAttempted = true;
	CreateResult.MaterialBindings.bAllRequestedVerified = false;

	FUeremcpNiagaraRoundTripResult RoundTrip;
	RoundTrip.bInspectSucceeded = true;
	RoundTrip.bStructuralMatch = true;
	RoundTrip.InspectGraphs = Graphs;

	const FUeremcpNiagaraPocBGateResult Gates =
		FUeremcpNiagaraPocBGates::Evaluate(CreateResult, &RoundTrip);

	TestTrue(TEXT("renderers present"), Gates.bB7RenderersPresent);
	TestTrue(TEXT("renderers bound evaluated"), Gates.bB7RenderersBoundEvaluated);
	TestFalse(TEXT("renderers bound without verify"), Gates.bB7RenderersBound);
	TestTrue(TEXT("data interfaces evaluated"), Gates.bB7DataInterfacesEvaluated);
	TestFalse(TEXT("data interfaces not complete"), Gates.bB7DataInterfacesComplete);

	const TSharedPtr<FJsonObject> Diagnostics = FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(Gates);
	const TSharedPtr<FJsonObject>* InspectFidelity = nullptr;
	TestTrue(TEXT("inspect_fidelity object"), Diagnostics->TryGetObjectField(TEXT("inspect_fidelity"), InspectFidelity));

	bool bRenderersBound = true;
	TestTrue(TEXT("B7_renderers_bound field"), Diagnostics->TryGetBoolField(TEXT("B7_renderers_bound"), bRenderersBound));
	TestFalse(TEXT("B7_renderers_bound false"), bRenderersBound);

	bool bDataInterfacesComplete = true;
	TestTrue(
		TEXT("B7_data_interfaces_complete field"),
		Diagnostics->TryGetBoolField(TEXT("B7_data_interfaces_complete"), bDataInterfacesComplete));
	TestFalse(TEXT("B7_data_interfaces_complete false"), bDataInterfacesComplete);

	CreateResult.MaterialBindings.bAllRequestedVerified = true;
	const FUeremcpNiagaraPocBGateResult VerifiedGates =
		FUeremcpNiagaraPocBGates::Evaluate(CreateResult, &RoundTrip);
	TestTrue(TEXT("renderers bound when verified"), VerifiedGates.bB7RenderersBound);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
