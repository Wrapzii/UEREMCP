// UEREMCP — POC B inspect fidelity signals for gate scaffolding (WS-07).

#include "UeremcpNiagaraPocBInspectFidelity.h"

#include "UeremcpNiagaraInspectMapping.h"

namespace
{
	int32 CountRendererRefsOnEmitterGraph(const TSharedPtr<FJsonObject>& EmitterGraph)
	{
		if (!EmitterGraph.IsValid())
		{
			return 0;
		}

		const TSharedPtr<FJsonObject>* Extensions = nullptr;
		if (EmitterGraph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions && Extensions->IsValid())
		{
			const TSharedPtr<FJsonObject>* Niagara = nullptr;
			if ((*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) && Niagara && Niagara->IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Renderers = nullptr;
				if ((*Niagara)->TryGetArrayField(TEXT("renderers"), Renderers) && Renderers)
				{
					return Renderers->Num();
				}
			}
		}

		int32 NodeCount = 0;
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (EmitterGraph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
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
					++NodeCount;
				}
			}
		}

		return NodeCount;
	}

	int32 CountExtractedMaterialPathsOnEmitterGraph(const TSharedPtr<FJsonObject>& EmitterGraph)
	{
		if (!EmitterGraph.IsValid())
		{
			return 0;
		}

		const TSharedPtr<FJsonObject>* Extensions = nullptr;
		if (!EmitterGraph->TryGetObjectField(TEXT("extensions"), Extensions) || !Extensions || !Extensions->IsValid())
		{
			return 0;
		}

		const TSharedPtr<FJsonObject>* Niagara = nullptr;
		if (!(*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) || !Niagara || !Niagara->IsValid())
		{
			return 0;
		}

		const TArray<TSharedPtr<FJsonValue>>* Renderers = nullptr;
		if (!(*Niagara)->TryGetArrayField(TEXT("renderers"), Renderers) || !Renderers)
		{
			return 0;
		}

		int32 Count = 0;
		for (const TSharedPtr<FJsonValue>& RendererValue : *Renderers)
		{
			const TSharedPtr<FJsonObject> Renderer = RendererValue->AsObject();
			if (!Renderer.IsValid())
			{
				continue;
			}
			FString MaterialPath;
			if (Renderer->TryGetStringField(TEXT("material_path"), MaterialPath) && !MaterialPath.IsEmpty())
			{
				++Count;
			}
		}
		return Count;
	}

	TMap<FString, TSharedPtr<FJsonObject>> IndexEmitterGraphsByName(
		const TArray<TSharedPtr<FJsonValue>>& InspectGraphs)
	{
		TMap<FString, TSharedPtr<FJsonObject>> ByName;
		for (const TSharedPtr<FJsonValue>& GraphValue : InspectGraphs)
		{
			const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
			if (!Graph.IsValid())
			{
				continue;
			}

			FString GraphType;
			if (!Graph->TryGetStringField(TEXT("graph_type"), GraphType)
				|| GraphType != TEXT("NiagaraEmitterGraph"))
			{
				continue;
			}

			FString GraphName;
			if (Graph->TryGetStringField(TEXT("graph_name"), GraphName) && !GraphName.IsEmpty())
			{
				ByName.Add(GraphName, Graph);
			}
		}
		return ByName;
	}
}

FUeremcpNiagaraPocBInspectSignals FUeremcpNiagaraPocBInspectFidelity::Evaluate(
	const TArray<FString>& ExpectedEmitterNames,
	const TArray<TSharedPtr<FJsonValue>>& InspectGraphs)
{
	FUeremcpNiagaraPocBInspectSignals Out;
	Out.bEvaluated = InspectGraphs.Num() > 0;
	Out.ExpectedEmitterCount = ExpectedEmitterNames.Num();

	if (!Out.bEvaluated)
	{
		Out.FidelityNotes.Add(TEXT("No inspect graphs supplied."));
		return Out;
	}

	const TMap<FString, TSharedPtr<FJsonObject>> EmitterGraphs = IndexEmitterGraphsByName(InspectGraphs);

	for (const FString& EmitterName : ExpectedEmitterNames)
	{
		const TSharedPtr<FJsonObject>* Found = EmitterGraphs.Find(EmitterName);
		const int32 RendererCount = Found ? CountRendererRefsOnEmitterGraph(*Found) : 0;
		if (RendererCount > 0)
		{
			++Out.EmittersWithRendererRefs;
			Out.TotalRendererRefs += RendererCount;
			if (Found)
			{
				Out.RenderersWithExtractedMaterialPath += CountExtractedMaterialPathsOnEmitterGraph(*Found);
			}
		}
		else
		{
			Out.EmittersMissingRenderers.Add(EmitterName);
		}
	}

	if (Out.RenderersWithExtractedMaterialPath > 0)
	{
		Out.FidelityNotes.Add(
			TEXT("material_path values are extracted from GetRendererData propertyValues and are not validated (renderer_material_bindings)."));
	}

	const TSharedPtr<FJsonObject> SystemGraph = FUeremcpNiagaraInspectMapping::FindSystemGraph(InspectGraphs);
	if (SystemGraph.IsValid())
	{
		const TSharedPtr<FJsonObject>* Extensions = nullptr;
		if (SystemGraph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions && Extensions->IsValid())
		{
			const TSharedPtr<FJsonObject>* Niagara = nullptr;
			if ((*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) && Niagara && Niagara->IsValid())
			{
				const TSharedPtr<FJsonObject>* Dependencies = nullptr;
				if ((*Niagara)->TryGetObjectField(TEXT("dependencies"), Dependencies) && Dependencies && Dependencies->IsValid())
				{
					Out.bDependenciesPresent = true;
					double UsedDataInterfaces = 0.0;
					if ((*Dependencies)->TryGetNumberField(TEXT("used_data_interfaces"), UsedDataInterfaces))
					{
						Out.UsedDataInterfaces = static_cast<int32>(UsedDataInterfaces);
					}
				}

				const TSharedPtr<FJsonObject>* Compile = nullptr;
				if ((*Niagara)->TryGetObjectField(TEXT("compile"), Compile) && Compile && Compile->IsValid())
				{
					Out.bCompileStatePresent = true;
					(*Compile)->TryGetBoolField(TEXT("bHasErrors"), Out.bCompileHasErrors);
				}
			}
		}
	}

	if (!Out.bDependenciesPresent)
	{
		Out.FidelityNotes.Add(TEXT("System dependencies not present on inspect graph; include_dependencies required for DI observability."));
	}

	return Out;
}
