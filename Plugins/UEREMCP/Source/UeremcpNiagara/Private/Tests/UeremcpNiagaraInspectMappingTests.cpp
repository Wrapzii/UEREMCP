// Editor automation tests for UeremcpNiagara inspect mapping (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraRendererProperties.h"
#include "UeremcpNiagaraInspectMapping.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraEventHandlerPlaceholderTest,
	"UEREMCP.Niagara.Inspect.EventHandlerPlaceholders",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraEventHandlerPlaceholderTest::RunTest(const FString& Parameters)
{
	FNiagaraExt_StackIssues Issues;
	FNiagaraExt_StackIssue Issue;
	Issue.StackDisplayPath = TEXT("ProbeBurst/Event Handler - Source: DeathEvent");
	Issue.Location.EmitterName = FName(TEXT("ProbeBurst"));
	Issue.Location.ScriptName = FName(TEXT("ParticleEventScript"));
	Issues.Issues.Add(Issue);

	FNiagaraExt_SystemCompileState Compile;
	FNiagaraExt_ScriptCompileInfo ScriptInfo;
	ScriptInfo.EmitterName = FName(TEXT("ProbeBurst"));
	ScriptInfo.ScriptName = FName(TEXT("ParticleEventScript"));
	Compile.Scripts.Add(ScriptInfo);

	const TArray<TSharedPtr<FJsonValue>> Handlers =
		FUeremcpNiagaraInspectMapping::BuildEventHandlerPlaceholders(Issues, Compile);

	TestTrue(TEXT("at least one event handler placeholder"), Handlers.Num() >= 1);
	if (Handlers.Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Handler = Handlers[0]->AsObject();
	TestTrue(TEXT("handler object"), Handler.IsValid());
	if (!Handler.IsValid())
	{
		return false;
	}

	FString ScriptUsage;
	TestTrue(TEXT("script_usage ParticleEventScript"), Handler->TryGetStringField(TEXT("script_usage"), ScriptUsage));
	TestEqual(TEXT("script_usage value"), ScriptUsage, FString(TEXT("ParticleEventScript")));

	const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
	TestTrue(TEXT("modules array present"), Handler->TryGetArrayField(TEXT("modules"), Modules));
	TestTrue(TEXT("modules empty (lossy)"), Modules && Modules->Num() == 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraRendererMappingOfflineTest,
	"UEREMCP.Niagara.Inspect.RendererMappingOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraRendererMappingOfflineTest::RunTest(const FString& Parameters)
{
	FNiagaraExt_EmitterTopology Topology;
	FNiagaraExt_RendererRef RendererRef;
	RendererRef.RendererIndex = 0;
	RendererRef.RendererClass = UNiagaraRendererProperties::StaticClass();
	Topology.Renderers.Add(RendererRef);

	const TArray<TSharedPtr<FJsonValue>> Nodes =
		FUeremcpNiagaraInspectMapping::BuildRendererGraphNodes(TEXT("ProbeBurst"), Topology);
	TestEqual(TEXT("one renderer node"), Nodes.Num(), 1);

	const TSharedPtr<FJsonObject> Fidelity =
		FUeremcpNiagaraInspectMapping::MakeEmitterGraphFidelity(true);
	const TArray<TSharedPtr<FJsonValue>>* Lossy = nullptr;
	TestTrue(TEXT("fidelity lossy_areas"), Fidelity->TryGetArrayField(TEXT("lossy_areas"), Lossy));
	TestTrue(TEXT("includes renderer_material_bindings"), Lossy && Lossy->Num() >= 4);

	const FString MaterialPath = FUeremcpNiagaraInspectMapping::TryExtractMaterialPath(
		TEXT("{\"Material\":{\"asset_path\":\"/Game/Materials/M_Test\"}}"));
	TestEqual(TEXT("material path extracted"), MaterialPath, FString(TEXT("/Game/Materials/M_Test")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
