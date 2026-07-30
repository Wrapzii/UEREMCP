// Editor automation tests for UeremcpNiagara inspect (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraInspect.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraInspectPathGuardTest,
	"UEREMCP.Niagara.Inspect.PathGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraInspectPathGuardTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("tests root allowed"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(TEXT("/Game/__UeremcpTests/NS_WS07_Probe")));
	TestFalse(TEXT("game content rejected"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(TEXT("/Game/VFX/NS_Fireball")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraInspectSystemRuntimeTest,
	"UEREMCP.Niagara.Inspect.NS_WS07_Probe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraInspectSystemRuntimeTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-inspect-1","action":"inspect_system","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_Probe"},"options":{"response_detail":"complete"}})");

	const FString Json = UUeremcpNiagaraToolset::InspectSystem(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("inspect returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("status partially_completed"), Status, FString(TEXT("partially_completed")));

	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	TestTrue(TEXT("diagnostics present"), Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics) && Diagnostics && Diagnostics->IsValid());
	if (!Diagnostics || !Diagnostics->IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	TestTrue(TEXT("graphs array present"), (*Diagnostics)->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs);
	if (!Graphs || Graphs->Num() < 2)
	{
		AddError(TEXT("expected system + emitter graphs"));
		return false;
	}

	bool bFoundSystemGraph = false;
	bool bFoundModuleStack = false;
	bool bFoundRendererOnEmitterGraph = false;
	for (const TSharedPtr<FJsonValue>& GraphValue : *Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		Graph->TryGetStringField(TEXT("graph_type"), GraphType);
		if (GraphType == TEXT("NiagaraSystemGraph"))
		{
			bFoundSystemGraph = true;

			const TSharedPtr<FJsonObject>* Extensions = nullptr;
			if (Graph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions && Extensions->IsValid())
			{
				const TSharedPtr<FJsonObject>* Niagara = nullptr;
				if ((*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) && Niagara && Niagara->IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* EventHandlers = nullptr;
					TestTrue(
						TEXT("event_handlers array present on system graph"),
						(*Niagara)->TryGetArrayField(TEXT("event_handlers"), EventHandlers));
				}
			}
		}
		if (GraphType == TEXT("NiagaraModuleStack"))
		{
			bFoundModuleStack = true;
		}
		if (GraphType == TEXT("NiagaraEmitterGraph"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
			if (Graph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
			{
				for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
				{
					const TSharedPtr<FJsonObject> Node = NodeValue->AsObject();
					if (!Node.IsValid())
					{
						continue;
					}
					FString SemanticType;
					if (Node->TryGetStringField(TEXT("semantic_type"), SemanticType)
						&& SemanticType == TEXT("niagara_renderer"))
					{
						bFoundRendererOnEmitterGraph = true;
						break;
					}
				}
			}

			const TSharedPtr<FJsonObject>* Fidelity = nullptr;
			if (Graph->TryGetObjectField(TEXT("fidelity"), Fidelity) && Fidelity && Fidelity->IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Lossy = nullptr;
				if ((*Fidelity)->TryGetArrayField(TEXT("lossy_areas"), Lossy) && Lossy)
				{
					bool bHasRendererLossy = false;
					for (const TSharedPtr<FJsonValue>& AreaValue : *Lossy)
					{
						if (AreaValue->AsString() == TEXT("renderer_material_bindings"))
						{
							bHasRendererLossy = true;
							break;
						}
					}
					if (bFoundRendererOnEmitterGraph)
					{
						TestTrue(TEXT("emitter graph lists renderer_material_bindings"), bHasRendererLossy);
					}
				}
			}
		}
	}

	TestTrue(TEXT("NiagaraSystemGraph returned"), bFoundSystemGraph);
	TestTrue(TEXT("NiagaraModuleStack returned"), bFoundModuleStack);

	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	if (Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid())
	{
		const int32 InternalOps = static_cast<int32>((*Metrics)->GetNumberField(TEXT("internal_operations")));
		TestTrue(TEXT("internal_operations > 0"), InternalOps > 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
