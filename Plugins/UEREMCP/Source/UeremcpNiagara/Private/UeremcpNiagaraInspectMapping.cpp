// UEREMCP — Inspect graph mapping helpers (WS-07).

#include "UeremcpNiagaraInspectMapping.h"

#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraMaterialBinding.h"
#include "UeremcpNiagaraRendererResolve.h"
#include "NiagaraEmitter.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString MakeEventHandlerKey(const FName& EmitterName, const FName& ScriptName, const FString& DisplayPath)
	{
		return FString::Printf(TEXT("%s|%s|%s"), *EmitterName.ToString(), *ScriptName.ToString(), *DisplayPath);
	}

	TSharedPtr<FJsonObject> MakeEventHandlerPlaceholder(
		const FName& EmitterName,
		const FName& ScriptName,
		const FString& EventHint,
		const FString& InferredFrom)
	{
		TSharedPtr<FJsonObject> Handler = MakeShared<FJsonObject>();
		if (!EventHint.IsEmpty())
		{
			Handler->SetStringField(TEXT("event_name"), EventHint);
		}
		if (!EmitterName.IsNone())
		{
			Handler->SetStringField(TEXT("source_emitter"), EmitterName.ToString());
		}
		if (!ScriptName.IsNone())
		{
			Handler->SetStringField(TEXT("script_usage"), ScriptName.ToString());
		}
		Handler->SetStringField(TEXT("inferred_from"), InferredFrom);
		Handler->SetStringField(
			TEXT("fidelity_note"),
			TEXT("Placeholder only — prefer GetEventHandlers() for metadata; "
				 "module stacks not addressable via ExternalEditUtilities (no UsageId)."));
		Handler->SetArrayField(TEXT("modules"), TArray<TSharedPtr<FJsonValue>>());
		return Handler;
	}

	FString ExtractEventHintFromDisplayPath(const FString& StackDisplayPath)
	{
		int32 HandlerIdx = StackDisplayPath.Find(TEXT("Event Handler"), ESearchCase::IgnoreCase);
		if (HandlerIdx == INDEX_NONE)
		{
			return FString();
		}
		FString Tail = StackDisplayPath.Mid(HandlerIdx);
		Tail.ReplaceInline(TEXT("Event Handler"), TEXT(""));
		Tail.TrimStartAndEndInline();
		Tail.RemoveFromStart(TEXT("-"));
		Tail.RemoveFromStart(TEXT(":"));
		Tail.TrimStartAndEndInline();
		return Tail;
	}

	FString ReadMaterialPathField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		if (!Object.IsValid())
		{
			return FString();
		}

		FString Direct;
		if (Object->TryGetStringField(FieldName, Direct) && !Direct.IsEmpty())
		{
			return Direct;
		}

		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Object->TryGetObjectField(FieldName, Nested) && Nested && Nested->IsValid())
		{
			FString AssetPath;
			if ((*Nested)->TryGetStringField(TEXT("asset_path"), AssetPath))
			{
				return AssetPath;
			}
			if ((*Nested)->TryGetStringField(TEXT("AssetPath"), AssetPath))
			{
				return AssetPath;
			}
			if ((*Nested)->TryGetStringField(TEXT("ObjectPath"), AssetPath))
			{
				return AssetPath;
			}
		}

		return FString();
	}
}

TSharedPtr<FJsonObject> FUeremcpNiagaraInspectMapping::MakeEmitterGraphFidelity(bool bHasRenderers)
{
	TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
	Fidelity->SetBoolField(TEXT("round_trip_supported"), false);

	TArray<TSharedPtr<FJsonValue>> Lossy;
	for (const FString& Area : UeremcpNiagaraCapability::EmitterGraphLossyAreas(bHasRenderers))
	{
		Lossy.Add(MakeShared<FJsonValueString>(Area));
	}
	Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
	return Fidelity;
}

FString FUeremcpNiagaraInspectMapping::TryExtractMaterialPath(const FString& PropertyValuesJson)
{
	if (PropertyValuesJson.IsEmpty())
	{
		return FString();
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertyValuesJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return FString();
	}

	static const TCHAR* MaterialFields[] = {
		TEXT("Material"),
		TEXT("MaterialOverride"),
		TEXT("MaterialUserParamBinding"),
		TEXT("MaterialInterface"),
	};

	for (const TCHAR* FieldName : MaterialFields)
	{
		const FString Path = ReadMaterialPathField(Root, FieldName);
		if (!Path.IsEmpty())
		{
			return Path;
		}
	}

	return FString();
}

TArray<TSharedPtr<FJsonValue>> FUeremcpNiagaraInspectMapping::BuildRendererExtensionEntries(
	UNiagaraSystem* System,
	const FName& EmitterName,
	const FNiagaraExt_EmitterTopology& Topology,
	FNiagaraExternalEditContext& Context,
	int32& InOutInternalOperations,
	bool& bOutFetchedPropertyValues)
{
	TArray<TSharedPtr<FJsonValue>> Renderers;
	bOutFetchedPropertyValues = false;

	for (const FNiagaraExt_RendererRef& Renderer : Topology.Renderers)
	{
		TSharedPtr<FJsonObject> RendererObj = MakeShared<FJsonObject>();
		RendererObj->SetNumberField(TEXT("renderer_index"), Renderer.RendererIndex);
		if (Renderer.RendererClass)
		{
			RendererObj->SetStringField(TEXT("renderer_class"), Renderer.RendererClass->GetPathName());
		}

		FNiagaraExt_StackItemReference RendererRef(System, EmitterName);
		RendererRef.RendererIndex = Renderer.RendererIndex;

		const bool bMeshRendererClass = Renderer.RendererClass
			&& Renderer.RendererClass->IsChildOf(UNiagaraMeshRendererProperties::StaticClass());

		UNiagaraMeshRendererProperties* MeshProps =
			UeremcpNiagaraRendererResolve::GetMeshRendererAtIndex(
				System,
				EmitterName,
				Renderer.RendererIndex);
		if (!MeshProps && bMeshRendererClass)
		{
			MeshProps = Cast<UNiagaraMeshRendererProperties>(
				UeremcpNiagaraRendererResolve::GetRendererAtIndex(
					System,
					EmitterName,
					Renderer.RendererIndex));
		}

		if (MeshProps)
		{
			++InOutInternalOperations;
			bOutFetchedPropertyValues = true;

			const TSharedPtr<FJsonObject> PropertyValues =
				FUeremcpNiagaraMaterialBinding::BuildMeshRendererObservabilityPropertyValues(MeshProps);
			if (PropertyValues.IsValid())
			{
				RendererObj->SetObjectField(TEXT("property_values"), PropertyValues);
			}

			const FString MaterialPath =
				FUeremcpNiagaraMaterialBinding::ExtractMaterialPathFromMeshRenderer(MeshProps);
			if (!MaterialPath.IsEmpty())
			{
				RendererObj->SetStringField(TEXT("material_path"), MaterialPath);
				RendererObj->SetStringField(
					TEXT("material_path_fidelity"),
					TEXT("extracted_from_mesh_renderer_fields_not_validated"));
			}
		}
		else if (bMeshRendererClass)
		{
			// Never GetRendererData on mesh: GetAllObjectProperties(Renderer) evaluates
			// FNiagaraMeshMaterialOverride::ExplicitMat edit conditions in struct scope.
			// [VERIFIED: GetRendererDataInternal — NiagaraExternalSystemEditorUtilities.cpp:1881]
			RendererObj->SetStringField(
				TEXT("property_values_fidelity"),
				TEXT("mesh_renderer_unresolved; GetRendererData skipped (OverrideMaterials EditCondition)"));
		}
		else
		{
			FNiagaraExt_RendererData RendererData;
			UNiagaraExternalEditUtilities::GetRendererData(RendererRef, RendererData, Context);
			++InOutInternalOperations;
			bOutFetchedPropertyValues = true;

			if (!RendererData.PropertyValues.IsEmpty())
			{
				TSharedPtr<FJsonObject> ParsedValues;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RendererData.PropertyValues);
				if (FJsonSerializer::Deserialize(Reader, ParsedValues) && ParsedValues.IsValid())
				{
					RendererObj->SetObjectField(TEXT("property_values"), ParsedValues);
				}
				else
				{
					RendererObj->SetStringField(TEXT("property_values_json"), RendererData.PropertyValues);
				}

				const FString MaterialPath = TryExtractMaterialPath(RendererData.PropertyValues);
				if (!MaterialPath.IsEmpty())
				{
					RendererObj->SetStringField(TEXT("material_path"), MaterialPath);
					RendererObj->SetStringField(
						TEXT("material_path_fidelity"),
						TEXT("extracted_from_property_values_not_validated"));
				}
			}
		}

		Renderers.Add(MakeShared<FJsonValueObject>(RendererObj));
	}

	return Renderers;
}

TArray<TSharedPtr<FJsonValue>> FUeremcpNiagaraInspectMapping::BuildRendererGraphNodes(
	const FString& EmitterName,
	const FNiagaraExt_EmitterTopology& Topology)
{
	TArray<TSharedPtr<FJsonValue>> Nodes;

	for (const FNiagaraExt_RendererRef& Renderer : Topology.Renderers)
	{
		TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
		const FString SemanticId = FString::Printf(
			TEXT("%s/Renderer/%d"),
			*EmitterName,
			Renderer.RendererIndex);
		Node->SetStringField(TEXT("node_id"), FString::Printf(TEXT("renderer_%s_%d"), *EmitterName, Renderer.RendererIndex));
		Node->SetStringField(TEXT("semantic_id"), SemanticId);
		Node->SetStringField(TEXT("semantic_type"), TEXT("niagara_renderer"));
		Node->SetNumberField(TEXT("renderer_index"), Renderer.RendererIndex);
		if (Renderer.RendererClass)
		{
			Node->SetStringField(TEXT("node_class"), Renderer.RendererClass->GetName());
			Node->SetStringField(TEXT("title"), Renderer.RendererClass->GetName());
		}
		Nodes.Add(MakeShared<FJsonValueObject>(Node));
	}

	return Nodes;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraInspectMapping::BuildScriptGraphInternalsSummary(
	UNiagaraScript* ModuleScript)
{
	if (!ModuleScript)
	{
		return nullptr;
	}

	UNiagaraScriptSourceBase* SourceBase = ModuleScript->GetLatestSource();
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
	if (!Source || !Source->NodeGraph)
	{
		TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
		Empty->SetStringField(TEXT("fidelity"), TEXT("no_script_source_graph"));
		Empty->SetBoolField(TEXT("write_supported"), false);
		return Empty;
	}

	int32 NodeCount = 0;
	int32 FunctionCallCount = 0;
	int32 CustomHlslCount = 0;
	TArray<TSharedPtr<FJsonValue>> SampleNodes;
	for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		++NodeCount;
		if (Cast<UNiagaraNodeCustomHlsl>(Node))
		{
			++CustomHlslCount;
		}
		if (UNiagaraNodeFunctionCall* Fn = Cast<UNiagaraNodeFunctionCall>(Node))
		{
			++FunctionCallCount;
			if (SampleNodes.Num() < 16)
			{
				TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
				Sample->SetStringField(TEXT("function_name"), Fn->GetFunctionName());
				if (Fn->FunctionScript)
				{
					Sample->SetStringField(TEXT("script"), Fn->FunctionScript->GetPathName());
				}
				SampleNodes.Add(MakeShared<FJsonValueObject>(Sample));
			}
		}
	}

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("node_count"), NodeCount);
	Summary->SetNumberField(TEXT("function_call_count"), FunctionCallCount);
	Summary->SetNumberField(TEXT("custom_hlsl_node_count"), CustomHlslCount);
	Summary->SetArrayField(TEXT("sample_function_calls"), SampleNodes);
	Summary->SetBoolField(TEXT("write_supported"), false);
	Summary->SetStringField(
		TEXT("write_blocker"),
		TEXT("UNiagaraExternalEditUtilities has no NiagaraScriptGraph / EdGraph mutate API; "
			 "NodeGraph is read-only via UNiagaraScriptSource::NodeGraph UPROPERTY."));
	Summary->SetStringField(TEXT("fidelity"), TEXT("read_summary_only"));
	return Summary;
}

TArray<TSharedPtr<FJsonValue>> FUeremcpNiagaraInspectMapping::BuildEventHandlersFromEmitterData(
	UNiagaraSystem* System,
	int32& InOutInternalOperations)
{
	TArray<TSharedPtr<FJsonValue>> Handlers;
	if (!System)
	{
		return Handlers;
	}

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (!EmitterData)
		{
			continue;
		}
		++InOutInternalOperations;

		// [VERIFIED: GetEventHandlers — NiagaraEmitter.h:427]
		for (const FNiagaraEventScriptProperties& EventProps : EmitterData->GetEventHandlers())
		{
			TSharedPtr<FJsonObject> Handler = MakeShared<FJsonObject>();
			Handler->SetStringField(TEXT("source_emitter"), Handle.GetName().ToString());
			Handler->SetStringField(TEXT("script_usage"), TEXT("ParticleEventScript"));
			Handler->SetStringField(TEXT("event_name"), EventProps.SourceEventName.ToString());
			Handler->SetStringField(
				TEXT("source_emitter_id"),
				EventProps.SourceEmitterID.ToString(EGuidFormats::DigitsWithHyphens));
			if (EventProps.Script)
			{
				Handler->SetStringField(
					TEXT("usage_guid"),
					EventProps.Script->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
				Handler->SetStringField(TEXT("script_path"), EventProps.Script->GetPathName());
			}
			if (const UEnum* ExecEnum = StaticEnum<EScriptExecutionMode>())
			{
				Handler->SetStringField(
					TEXT("execution_mode"),
					ExecEnum->GetNameStringByValue(static_cast<int64>(EventProps.ExecutionMode)));
			}
			Handler->SetNumberField(TEXT("spawn_number"), static_cast<double>(EventProps.SpawnNumber));
			Handler->SetNumberField(
				TEXT("max_events_per_frame"),
				static_cast<double>(EventProps.MaxEventsPerFrame));
			Handler->SetStringField(TEXT("inferred_from"), TEXT("FVersionedNiagaraEmitterData::GetEventHandlers"));
			Handler->SetStringField(
				TEXT("fidelity_note"),
				TEXT("Metadata from GetEventHandlers. Module list is NodeGraph best-effort; "
					 "ExternalEditUtilities cannot AddModule/SetModuleEnabled on ParticleEventScript "
					 "(StackItemReference has no UsageId; FindScriptGroup needs Guid)."));

			TArray<TSharedPtr<FJsonValue>> Modules;
			if (EventProps.Script)
			{
				if (TSharedPtr<FJsonObject> GraphSummary =
					BuildScriptGraphInternalsSummary(EventProps.Script))
				{
					Handler->SetObjectField(TEXT("script_graph_internals"), GraphSummary);
					const TArray<TSharedPtr<FJsonValue>>* Samples = nullptr;
					if (GraphSummary->TryGetArrayField(TEXT("sample_function_calls"), Samples)
						&& Samples)
					{
						for (const TSharedPtr<FJsonValue>& Sample : *Samples)
						{
							Modules.Add(Sample);
						}
					}
				}
			}
			Handler->SetArrayField(TEXT("modules"), Modules);
			Handlers.Add(MakeShared<FJsonValueObject>(Handler));
		}
	}
	return Handlers;
}

TArray<TSharedPtr<FJsonValue>> FUeremcpNiagaraInspectMapping::BuildEventHandlerPlaceholders(
	const FNiagaraExt_StackIssues& Issues,
	const FNiagaraExt_SystemCompileState& CompileState)
{
	TArray<TSharedPtr<FJsonValue>> Handlers;
	TSet<FString> Seen;

	auto AddHandler = [&](const TSharedPtr<FJsonObject>& Handler)
	{
		if (!Handler.IsValid())
		{
			return;
		}
		FString Emitter;
		FString Script;
		FString Hint;
		Handler->TryGetStringField(TEXT("source_emitter"), Emitter);
		Handler->TryGetStringField(TEXT("script_usage"), Script);
		Handler->TryGetStringField(TEXT("event_name"), Hint);
		const FString Key = MakeEventHandlerKey(FName(*Emitter), FName(*Script), Hint);
		if (Seen.Contains(Key))
		{
			return;
		}
		Seen.Add(Key);
		Handlers.Add(MakeShared<FJsonValueObject>(Handler));
	};

	for (const FNiagaraExt_StackIssue& Issue : Issues.Issues)
	{
		const bool bEventPath = Issue.StackDisplayPath.Contains(TEXT("Event Handler"), ESearchCase::IgnoreCase)
			|| Issue.StackDisplayPath.Contains(TEXT("ParticleEventScript"), ESearchCase::IgnoreCase)
			|| Issue.ShortDescription.Contains(TEXT("Event Handler"), ESearchCase::IgnoreCase)
			|| Issue.Location.ScriptName == FName(TEXT("ParticleEventScript"));

		if (!bEventPath)
		{
			continue;
		}

		const FString Hint = ExtractEventHintFromDisplayPath(Issue.StackDisplayPath);
		AddHandler(MakeEventHandlerPlaceholder(
			Issue.Location.EmitterName,
			Issue.Location.ScriptName.IsNone() ? FName(TEXT("ParticleEventScript")) : Issue.Location.ScriptName,
			Hint,
			TEXT("GetStackIssues")));
	}

	for (const FNiagaraExt_ScriptCompileInfo& ScriptInfo : CompileState.Scripts)
	{
		if (ScriptInfo.ScriptName != FName(TEXT("ParticleEventScript")))
		{
			continue;
		}

		AddHandler(MakeEventHandlerPlaceholder(
			ScriptInfo.EmitterName,
			ScriptInfo.ScriptName,
			FString(),
			TEXT("GetSystemCompileState.per_script")));
	}

	return Handlers;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraInspectMapping::FindSystemGraph(
	const TArray<TSharedPtr<FJsonValue>>& Graphs)
{
	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		if (Graph->TryGetStringField(TEXT("graph_type"), GraphType)
			&& GraphType == TEXT("NiagaraSystemGraph"))
		{
			return Graph;
		}
	}
	return nullptr;
}

int32 FUeremcpNiagaraInspectMapping::CountEmitterGraphs(const TArray<TSharedPtr<FJsonValue>>& Graphs)
{
	int32 Count = 0;
	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		if (Graph->TryGetStringField(TEXT("graph_type"), GraphType)
			&& GraphType == TEXT("NiagaraEmitterGraph"))
		{
			++Count;
		}
	}
	return Count;
}

TArray<FString> FUeremcpNiagaraInspectMapping::ReadUserParameterNames(
	const TSharedPtr<FJsonObject>& SystemGraph)
{
	TArray<FString> Names;
	if (!SystemGraph.IsValid())
	{
		return Names;
	}

	const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
	if (SystemGraph->TryGetArrayField(TEXT("variables"), Variables) && Variables)
	{
		for (const TSharedPtr<FJsonValue>& VarValue : *Variables)
		{
			const TSharedPtr<FJsonObject> VarObj = VarValue->AsObject();
			if (!VarObj.IsValid())
			{
				continue;
			}
			FString Name;
			if (VarObj->TryGetStringField(TEXT("name"), Name))
			{
				Names.Add(Name);
			}
		}
	}

	const TSharedPtr<FJsonObject>* Extensions = nullptr;
	if (SystemGraph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions && Extensions->IsValid())
	{
		const TSharedPtr<FJsonObject>* Niagara = nullptr;
		if ((*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) && Niagara && Niagara->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* UserParams = nullptr;
			if ((*Niagara)->TryGetArrayField(TEXT("user_parameters"), UserParams) && UserParams)
			{
				for (const TSharedPtr<FJsonValue>& ParamValue : *UserParams)
				{
					const TSharedPtr<FJsonObject> ParamObj = ParamValue->AsObject();
					if (!ParamObj.IsValid())
					{
						continue;
					}
					FString Name;
					if (ParamObj->TryGetStringField(TEXT("name"), Name) && !Names.Contains(Name))
					{
						Names.Add(Name);
					}
				}
			}
		}
	}

	return Names;
}
