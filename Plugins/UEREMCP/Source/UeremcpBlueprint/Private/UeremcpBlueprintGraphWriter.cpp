#include "UeremcpBlueprintGraphWriter.h"

#include "UeremcpBlueprintEpicBridge.h"
#include "UeremcpBlueprintGraphReader.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace UeremcpBlueprintGraphWriter
{
	static TSharedPtr<FJsonObject> LookupNode(
		const TMap<FString, TSharedPtr<FJsonObject>>& NodeById,
		const FString& NodeId)
	{
		if (const TSharedPtr<FJsonObject>* Found = NodeById.Find(NodeId))
		{
			return *Found;
		}
		return nullptr;
	}

	static TMap<FString, TSharedPtr<FJsonObject>> BuildNodeMap(const TSharedPtr<FJsonObject>& Graph)
	{
		TMap<FString, TSharedPtr<FJsonObject>> NodeById;
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (!Graph->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes)
		{
			return NodeById;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Nodes)
		{
			const TSharedPtr<FJsonObject> Node = Value->AsObject();
			if (!Node.IsValid())
			{
				continue;
			}
			FString Id;
			if (Node->TryGetStringField(TEXT("node_id"), Id))
			{
				NodeById.Add(Id, Node);
			}
		}
		return NodeById;
	}

	static FString GetThenTarget(
		const TSharedPtr<FJsonObject>& Node,
		const TMap<FString, TSharedPtr<FJsonObject>>& NodeById)
	{
		const TArray<TSharedPtr<FJsonValue>>* OutputPins = nullptr;
		if (!Node->TryGetArrayField(TEXT("output_pins"), OutputPins) || !OutputPins)
		{
			return FString();
		}

		for (const TSharedPtr<FJsonValue>& PinValue : *OutputPins)
		{
			const TSharedPtr<FJsonObject> Pin = PinValue->AsObject();
			if (!Pin.IsValid())
			{
				continue;
			}
			FString PinName;
			if (!Pin->TryGetStringField(TEXT("name"), PinName) || !PinName.Equals(TEXT("then"), ESearchCase::CaseSensitive))
			{
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
			if (!Pin->TryGetArrayField(TEXT("links"), Links) || !Links || Links->Num() == 0)
			{
				return FString();
			}
			const TSharedPtr<FJsonObject> Link = (*Links)[0]->AsObject();
			if (!Link.IsValid())
			{
				return FString();
			}
			FString TargetNodeId;
			Link->TryGetStringField(TEXT("node_id"), TargetNodeId);
			return TargetNodeId;
		}
		return FString();
	}

	static bool IsEventEntryNode(const TSharedPtr<FJsonObject>& Node)
	{
		FString SemanticType;
		if (Node->TryGetStringField(TEXT("semantic_type"), SemanticType))
		{
			if (SemanticType.StartsWith(TEXT("event:")) || SemanticType.StartsWith(TEXT("custom_event:")))
			{
				return true;
			}
		}

		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (Node->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
		{
			FString TypeId;
			if ((*Properties)->TryGetStringField(TEXT("type_id"), TypeId))
			{
				return TypeId.StartsWith(TEXT("Event|")) || TypeId.StartsWith(TEXT("AddEvent|Custom|"));
			}
		}
		return false;
	}

	static bool ResolveEventName(const TSharedPtr<FJsonObject>& Node, FString& OutEventName)
	{
		FString SemanticType;
		if (Node->TryGetStringField(TEXT("semantic_type"), SemanticType))
		{
			if (SemanticType.StartsWith(TEXT("event:")))
			{
				OutEventName = SemanticType.Mid(6);
				return !OutEventName.IsEmpty();
			}
			if (SemanticType.StartsWith(TEXT("custom_event:")))
			{
				OutEventName = SemanticType.Mid(13);
				return !OutEventName.IsEmpty();
			}
		}

		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (Node->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
		{
			FString TypeId;
			if ((*Properties)->TryGetStringField(TEXT("type_id"), TypeId))
			{
				if (TypeId.Equals(TEXT("Event|ReceiveBeginPlay"), ESearchCase::CaseSensitive))
				{
					OutEventName = TEXT("EventBeginPlay");
					return true;
				}
				if (TypeId.StartsWith(TEXT("AddEvent|Custom|")))
				{
					OutEventName = TypeId.Mid(16);
					return !OutEventName.IsEmpty();
				}
			}
		}
		return false;
	}

	static FString FormatDefaultValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("");
		}
		if (Value->Type == EJson::String)
		{
			return FString::Printf(TEXT("\"%s\""), *Value->AsString().Replace(TEXT("\""), TEXT("\\\"")));
		}
		if (Value->Type == EJson::Boolean)
		{
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		}
		if (Value->Type == EJson::Number)
		{
			return LexToString(Value->AsNumber());
		}
		return TEXT("");
	}

	static bool IsUnsupportedControlFlowTypeId(const FString& TypeId)
	{
		return TypeId.Contains(TEXT("FlowControl|Branch"))
			|| TypeId.Contains(TEXT("FlowControl|IfThenElse"))
			|| TypeId.Contains(TEXT("|Switch"))
			|| TypeId.Contains(TEXT("MultiGate"))
			|| TypeId.Contains(TEXT("FlowControl|Sequence"));
	}

	static bool BuildCallInner(const TSharedPtr<FJsonObject>& Node, FString& OutInner, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (!Node->TryGetObjectField(TEXT("properties"), Properties) || !Properties || !Properties->IsValid())
		{
			OutError = TEXT("call node missing properties.type_id");
			return false;
		}

		FString TypeId;
		if (!(*Properties)->TryGetStringField(TEXT("type_id"), TypeId) || TypeId.IsEmpty())
		{
			OutError = TEXT("call node missing properties.type_id");
			return false;
		}

		if (IsUnsupportedControlFlowTypeId(TypeId))
		{
			OutError = FString::Printf(
				TEXT("graph JSON translator does not support control-flow node '%s'; provide extensions.blueprint.dsl"),
				*TypeId);
			return false;
		}

		TArray<FString> Args;
		Args.Add(TypeId);

		const TSharedPtr<FJsonObject>* Defaults = nullptr;
		if (Node->TryGetObjectField(TEXT("defaults"), Defaults) && Defaults && Defaults->IsValid())
		{
			TArray<FString> DefaultKeys;
			DefaultKeys.Reserve((*Defaults)->Values.Num());
			for (const auto& Pair : (*Defaults)->Values)
			{
				DefaultKeys.Add(FString(Pair.Key));
			}
			DefaultKeys.Sort();
			for (const FString& Key : DefaultKeys)
			{
				const TSharedPtr<FJsonValue> Value = (*Defaults)->TryGetField(Key);
				const FString Formatted = FormatDefaultValue(Value);
				if (!Formatted.IsEmpty())
				{
					Args.Add(FString::Printf(TEXT(":%s %s"), *Key, *Formatted));
				}
			}
		}

		OutInner = FString::Join(Args, TEXT(" "));
		return true;
	}

	static bool EmitLinearExecChain(
		const FString& StartNodeId,
		const TMap<FString, TSharedPtr<FJsonObject>>& NodeById,
		int32 Indent,
		FString& OutDsl,
		FString& OutError)
	{
		FString CurrentId = StartNodeId;
		TArray<FString> Segments;

		while (!CurrentId.IsEmpty())
		{
			const TSharedPtr<FJsonObject> Node = LookupNode(NodeById, CurrentId);
			if (!Node.IsValid())
			{
				OutError = FString::Printf(TEXT("exec chain references unknown node '%s'"), *CurrentId);
				return false;
			}

			if (IsEventEntryNode(Node))
			{
				CurrentId = GetThenTarget(Node, NodeById);
				continue;
			}

			FString Inner;
			if (!BuildCallInner(Node, Inner, OutError))
			{
				return false;
			}

			const FString Pad = FString::ChrN(Indent, TEXT(' '));
			const FString NextId = GetThenTarget(Node, NodeById);
			if (!NextId.IsEmpty())
			{
				FString Nested;
				if (!EmitLinearExecChain(NextId, NodeById, Indent + 2, Nested, OutError))
				{
					return false;
				}
				Segments.Add(FString::Printf(TEXT("%s(%s\n%s)"), *Pad, *Inner, *Nested));
				OutDsl = FString::Join(Segments, TEXT("\n"));
				return true;
			}

			Segments.Add(FString::Printf(TEXT("%s(%s)"), *Pad, *Inner));
			OutDsl = FString::Join(Segments, TEXT("\n"));
			return true;
		}

		OutDsl = FString();
		return true;
	}

	static bool TryTranslateGraphJsonToDsl(
		const TSharedPtr<FJsonObject>& Graph,
		FString& OutDsl,
		TArray<FString>& OutLossyNotes,
		FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* EntryPoints = nullptr;
		if (!Graph->TryGetArrayField(TEXT("entry_points"), EntryPoints) || !EntryPoints || EntryPoints->Num() == 0)
		{
			OutError = TEXT("graph JSON translation requires entry_points and extensions.blueprint.dsl is absent");
			return false;
		}

		const TMap<FString, TSharedPtr<FJsonObject>> NodeById = BuildNodeMap(Graph);
		TArray<FString> EventBlocks;

		for (const TSharedPtr<FJsonValue>& EntryValue : *EntryPoints)
		{
			const FString EntryNodeId = EntryValue->AsString();
			const TSharedPtr<FJsonObject> EntryNode = LookupNode(NodeById, EntryNodeId);
			if (!EntryNode.IsValid())
			{
				OutError = FString::Printf(TEXT("entry point '%s' not found in nodes"), *EntryNodeId);
				return false;
			}

			FString EventName;
			if (!ResolveEventName(EntryNode, EventName))
			{
				OutError = FString::Printf(
					TEXT("entry point '%s' is not a supported event/custom_event node"),
					*EntryNodeId);
				return false;
			}

			const FString BodyStart = GetThenTarget(EntryNode, NodeById);
			FString BodyDsl;
			if (!BodyStart.IsEmpty())
			{
				if (!EmitLinearExecChain(BodyStart, NodeById, 2, BodyDsl, OutError))
				{
					return false;
				}
			}

			if (BodyDsl.IsEmpty())
			{
				EventBlocks.Add(FString::Printf(TEXT("(event %s)"), *EventName));
			}
			else
			{
				EventBlocks.Add(FString::Printf(TEXT("(event %s\n%s)"), *EventName, *BodyDsl));
			}
		}

		OutDsl = FString::Join(EventBlocks, TEXT("\n"));
		OutLossyNotes.Add(TEXT("graph_json_to_dsl_minimal_translator"));
		OutLossyNotes.Add(TEXT("exotic_nodes_require_extensions_blueprint_dsl"));
		return true;
	}

	static bool ValidateExpectedAfterWriteShape(
		const TSharedPtr<FJsonObject>& Expected,
		FString& OutError)
	{
		if (!Expected.IsValid())
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
		if (!Expected->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes || Nodes->Num() == 0)
		{
			OutError = TEXT("expected_after_write.nodes must contain at least one node selector");
			return false;
		}
		if (!Expected->TryGetArrayField(TEXT("links"), Links) || !Links || Links->Num() == 0)
		{
			OutError = TEXT("expected_after_write.links must contain at least one link assertion");
			return false;
		}

		TSet<FString> Keys;
		for (const TSharedPtr<FJsonValue>& Value : *Nodes)
		{
			const TSharedPtr<FJsonObject> Selector = Value->AsObject();
			FString Key;
			if (!Selector.IsValid()
				|| !Selector->TryGetStringField(TEXT("key"), Key)
				|| Key.IsEmpty())
			{
				OutError = TEXT("each expected_after_write.nodes item requires a non-empty key");
				return false;
			}
			if (Keys.Contains(Key))
			{
				OutError = FString::Printf(TEXT("duplicate expected_after_write node key '%s'"), *Key);
				return false;
			}
			Keys.Add(Key);
			if (!Selector->HasField(TEXT("node_class"))
				&& !Selector->HasField(TEXT("semantic_type"))
				&& !Selector->HasField(TEXT("function")))
			{
				OutError = FString::Printf(
					TEXT("expected_after_write node '%s' requires node_class, semantic_type, or function"),
					*Key);
				return false;
			}
		}

		for (const TSharedPtr<FJsonValue>& Value : *Links)
		{
			const TSharedPtr<FJsonObject> Link = Value->AsObject();
			FString From;
			FString FromPin;
			FString To;
			FString ToPin;
			if (!Link.IsValid()
				|| !Link->TryGetStringField(TEXT("from"), From)
				|| !Link->TryGetStringField(TEXT("from_pin"), FromPin)
				|| !Link->TryGetStringField(TEXT("to"), To)
				|| !Link->TryGetStringField(TEXT("to_pin"), ToPin)
				|| From.IsEmpty()
				|| FromPin.IsEmpty()
				|| To.IsEmpty()
				|| ToPin.IsEmpty())
			{
				OutError = TEXT("each expected_after_write.links item requires from, from_pin, to, and to_pin");
				return false;
			}
			if (!Keys.Contains(From) || !Keys.Contains(To))
			{
				OutError = FString::Printf(
					TEXT("expected_after_write link references unknown node key '%s' or '%s'"),
					*From,
					*To);
				return false;
			}
		}

		return true;
	}

	static bool NodeMatchesSelector(
		const TSharedPtr<FJsonObject>& Node,
		const TSharedPtr<FJsonObject>& Selector)
	{
		for (const TCHAR* Field : {TEXT("node_class"), TEXT("semantic_type")})
		{
			FString ExpectedValue;
			if (Selector->TryGetStringField(Field, ExpectedValue))
			{
				FString ActualValue;
				if (!Node->TryGetStringField(Field, ActualValue)
					|| !ActualValue.Equals(ExpectedValue, ESearchCase::CaseSensitive))
				{
					return false;
				}
			}
		}

		FString ExpectedFunction;
		if (Selector->TryGetStringField(TEXT("function"), ExpectedFunction))
		{
			const TSharedPtr<FJsonObject>* Properties = nullptr;
			FString ActualFunction;
			if (!Node->TryGetObjectField(TEXT("properties"), Properties)
				|| !Properties
				|| !Properties->IsValid()
				|| !(*Properties)->TryGetStringField(TEXT("function"), ActualFunction)
				|| !ActualFunction.Equals(ExpectedFunction, ESearchCase::CaseSensitive))
			{
				return false;
			}
		}
		return true;
	}

	static bool ValidateExpectedAfterWrite(
		const TSharedPtr<FJsonObject>& Graph,
		const TSharedPtr<FJsonObject>& Expected,
		FString& OutError)
	{
		if (!Expected.IsValid())
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* ActualNodes = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* ExpectedNodes = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* ActualLinks = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* ExpectedLinks = nullptr;
		if (!Graph.IsValid()
			|| !Graph->TryGetArrayField(TEXT("nodes"), ActualNodes)
			|| !ActualNodes
			|| !Graph->TryGetArrayField(TEXT("links"), ActualLinks)
			|| !ActualLinks
			|| !Expected->TryGetArrayField(TEXT("nodes"), ExpectedNodes)
			|| !ExpectedNodes
			|| !Expected->TryGetArrayField(TEXT("links"), ExpectedLinks)
			|| !ExpectedLinks)
		{
			OutError = TEXT("post-write graph or expectation arrays are unavailable");
			return false;
		}

		TMap<FString, FString> NodeIdByKey;
		for (const TSharedPtr<FJsonValue>& SelectorValue : *ExpectedNodes)
		{
			const TSharedPtr<FJsonObject> Selector = SelectorValue->AsObject();
			FString Key;
			Selector->TryGetStringField(TEXT("key"), Key);
			TArray<FString> Matches;
			for (const TSharedPtr<FJsonValue>& NodeValue : *ActualNodes)
			{
				const TSharedPtr<FJsonObject> Node = NodeValue->AsObject();
				FString NodeId;
				if (Node.IsValid()
					&& NodeMatchesSelector(Node, Selector)
					&& Node->TryGetStringField(TEXT("node_id"), NodeId))
				{
					Matches.Add(NodeId);
				}
			}
			if (Matches.Num() != 1)
			{
				OutError = FString::Printf(
					TEXT("post-write node assertion '%s' matched %d nodes; expected exactly one"),
					*Key,
					Matches.Num());
				return false;
			}
			NodeIdByKey.Add(Key, Matches[0]);
		}

		for (const TSharedPtr<FJsonValue>& ExpectedLinkValue : *ExpectedLinks)
		{
			const TSharedPtr<FJsonObject> ExpectedLink = ExpectedLinkValue->AsObject();
			FString FromKey;
			FString FromPin;
			FString ToKey;
			FString ToPin;
			ExpectedLink->TryGetStringField(TEXT("from"), FromKey);
			ExpectedLink->TryGetStringField(TEXT("from_pin"), FromPin);
			ExpectedLink->TryGetStringField(TEXT("to"), ToKey);
			ExpectedLink->TryGetStringField(TEXT("to_pin"), ToPin);

			bool bFound = false;
			for (const TSharedPtr<FJsonValue>& ActualLinkValue : *ActualLinks)
			{
				const TSharedPtr<FJsonObject> ActualLink = ActualLinkValue->AsObject();
				FString FromNode;
				FString ActualFromPin;
				FString ToNode;
				FString ActualToPin;
				if (ActualLink.IsValid()
					&& ActualLink->TryGetStringField(TEXT("from_node"), FromNode)
					&& ActualLink->TryGetStringField(TEXT("from_pin"), ActualFromPin)
					&& ActualLink->TryGetStringField(TEXT("to_node"), ToNode)
					&& ActualLink->TryGetStringField(TEXT("to_pin"), ActualToPin)
					&& FromNode.Equals(NodeIdByKey[FromKey], ESearchCase::CaseSensitive)
					&& ActualFromPin.Equals(FromPin, ESearchCase::CaseSensitive)
					&& ToNode.Equals(NodeIdByKey[ToKey], ESearchCase::CaseSensitive)
					&& ActualToPin.Equals(ToPin, ESearchCase::CaseSensitive))
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				OutError = FString::Printf(
					TEXT("post-write link assertion failed: %s.%s -> %s.%s"),
					*FromKey,
					*FromPin,
					*ToKey,
					*ToPin);
				return false;
			}
		}

		return true;
	}
}

bool FUeremcpBlueprintGraphWriter::IsScratchAssetPath(const FString& AssetPath)
{
	return AssetPath.StartsWith(TEXT("/Game/__UeremcpTests/"), ESearchCase::CaseSensitive)
		|| AssetPath.StartsWith(TEXT("/Game/__UeremcpPoc/"), ESearchCase::CaseSensitive);
}

namespace UeremcpBlueprintGraphWriterValidate
{
	static bool RequireStringField(
		const TSharedPtr<FJsonObject>& Graph,
		const TCHAR* Field,
		FString& OutValue,
		FString& OutError)
	{
		if (!Graph->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("submitted graph missing required field '%s'"), Field);
			return false;
		}
		return true;
	}
}

bool FUeremcpBlueprintGraphWriter::ValidateSubmittedGraphForReplace(
	const TSharedPtr<FJsonObject>& SubmittedGraph,
	const FString& ExpectedAssetPath,
	const FString& ExpectedGraphId,
	FString& OutError,
	TArray<FString>& OutCapabilityNotes)
{
	using namespace UeremcpBlueprintGraphWriterValidate;

	OutError.Reset();
	OutCapabilityNotes.Reset();

	if (!SubmittedGraph.IsValid())
	{
		OutError = TEXT("submitted graph is null");
		return false;
	}

	FString AssetPath;
	FString GraphId;
	FString GraphType;
	FString SchemaVersion;
	if (!RequireStringField(SubmittedGraph, TEXT("asset_path"), AssetPath, OutError)
		|| !RequireStringField(SubmittedGraph, TEXT("graph_id"), GraphId, OutError)
		|| !RequireStringField(SubmittedGraph, TEXT("graph_type"), GraphType, OutError)
		|| !RequireStringField(SubmittedGraph, TEXT("schema_version"), SchemaVersion, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Fidelity = nullptr;
	if (!SubmittedGraph->TryGetObjectField(TEXT("fidelity"), Fidelity) || !Fidelity || !Fidelity->IsValid())
	{
		OutError = TEXT("submitted graph missing required object 'fidelity'");
		return false;
	}
	if (!(*Fidelity)->HasTypedField<EJson::Boolean>(TEXT("round_trip_supported")))
	{
		OutError = TEXT("submitted graph.fidelity.round_trip_supported is required");
		return false;
	}

	if (!GraphType.StartsWith(TEXT("Blueprint"), ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(
			TEXT("submit_graph replace supports Blueprint graph_type values only; got '%s'"),
			*GraphType);
		OutCapabilityNotes.Add(TEXT("submit_graph.unsupported_graph_type"));
		return false;
	}

	if (!ExpectedAssetPath.IsEmpty()
		&& !AssetPath.Equals(ExpectedAssetPath, ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(
			TEXT("submitted graph asset_path '%s' does not match target '%s'"),
			*AssetPath,
			*ExpectedAssetPath);
		OutCapabilityNotes.Add(TEXT("submit_graph.asset_path_mismatch"));
		return false;
	}

	if (!ExpectedGraphId.IsEmpty() && !GraphId.Equals(ExpectedGraphId, ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(
			TEXT("submitted graph graph_id '%s' does not match target '%s'"),
			*GraphId,
			*ExpectedGraphId);
		OutCapabilityNotes.Add(TEXT("submit_graph.graph_id_mismatch"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	if (!SubmittedGraph->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes)
	{
		OutError = TEXT("submitted graph missing required array 'nodes'");
		return false;
	}

	FString Dsl;
	TArray<FString> LossyNotes;
	if (!ResolveWriteDsl(SubmittedGraph, Dsl, LossyNotes, OutError))
	{
		OutCapabilityNotes.Add(TEXT("submit_graph.dsl_required"));
		return false;
	}
	if (Dsl.IsEmpty())
	{
		OutError = TEXT("resolved DSL is empty");
		OutCapabilityNotes.Add(TEXT("submit_graph.dsl_required"));
		return false;
	}

	return true;
}

bool FUeremcpBlueprintGraphWriter::ResolveWriteDsl(
	const TSharedPtr<FJsonObject>& SubmittedGraph,
	FString& OutDsl,
	TArray<FString>& OutLossyNotes,
	FString& OutError)
{
	if (!SubmittedGraph.IsValid())
	{
		OutError = TEXT("submitted graph is null");
		return false;
	}

	const TSharedPtr<FJsonObject>* ExtensionsPtr = nullptr;
	if (SubmittedGraph->TryGetObjectField(TEXT("extensions"), ExtensionsPtr)
		&& ExtensionsPtr
		&& ExtensionsPtr->IsValid())
	{
		const TSharedPtr<FJsonObject>* BlueprintExt = nullptr;
		if ((*ExtensionsPtr)->TryGetObjectField(TEXT("blueprint"), BlueprintExt)
			&& BlueprintExt
			&& BlueprintExt->IsValid())
		{
			FString Dsl;
			if ((*BlueprintExt)->TryGetStringField(TEXT("dsl"), Dsl) && !Dsl.IsEmpty())
			{
				OutDsl = Dsl;
				return true;
			}
		}
	}

	return UeremcpBlueprintGraphWriter::TryTranslateGraphJsonToDsl(
		SubmittedGraph,
		OutDsl,
		OutLossyNotes,
		OutError);
}

bool FUeremcpBlueprintGraphWriter::WriteIntentDiffers(
	const TSharedPtr<FJsonObject>& SubmittedGraph,
	const TSharedPtr<FJsonObject>& CurrentGraph)
{
	if (!SubmittedGraph.IsValid() || !CurrentGraph.IsValid())
	{
		return true;
	}

	auto ResolveDsl = [](const TSharedPtr<FJsonObject>& Graph, FString& OutDsl) -> bool
	{
		FString Error;
		TArray<FString> LossyNotes;
		return ResolveWriteDsl(Graph, OutDsl, LossyNotes, Error) && !OutDsl.IsEmpty();
	};

	FString SubmittedDsl;
	FString CurrentDsl;
	if (!ResolveDsl(SubmittedGraph, SubmittedDsl) || !ResolveDsl(CurrentGraph, CurrentDsl))
	{
		return true;
	}

	return !SubmittedDsl.Equals(CurrentDsl, ESearchCase::CaseSensitive);
}

bool FUeremcpBlueprintGraphWriter::ReplaceGraph(
	UBlueprint* Blueprint,
	const TSharedPtr<FJsonObject>& SubmittedGraph,
	const FUeremcpBlueprintReplaceGraphOptions& Options,
	FUeremcpBlueprintReplaceGraphResult& OutResult)
{
	OutResult = FUeremcpBlueprintReplaceGraphResult();
	OutResult.LossyAreas = FUeremcpBlueprintGraphReader::DefaultLossyAreas();

	if (!Blueprint)
	{
		OutResult.Error = TEXT("blueprint is null");
		return false;
	}

	if (!IsScratchAssetPath(Options.AssetPath))
	{
		OutResult.Error = TEXT("submit_graph replace writes are restricted to /Game/__UeremcpTests/ or /Game/__UeremcpPoc/ scratch assets");
		OutResult.CapabilityNotes.Add(TEXT("submit_graph.scratch_path_only"));
		return false;
	}

	FString ExpectedShapeError;
	if (!UeremcpBlueprintGraphWriter::ValidateExpectedAfterWriteShape(
			Options.ExpectedAfterWrite,
			ExpectedShapeError))
	{
		OutResult.Error = ExpectedShapeError;
		OutResult.CapabilityNotes.Add(TEXT("submit_graph.expected_after_write_invalid"));
		return false;
	}

	FString DslError;
	TArray<FString> DslLossyNotes;
	if (!ResolveWriteDsl(SubmittedGraph, OutResult.DslUsed, DslLossyNotes, DslError))
	{
		OutResult.Error = DslError;
		OutResult.CapabilityNotes.Add(TEXT("submit_graph.dsl_required"));
		return false;
	}
	OutResult.LossyAreas.Append(DslLossyNotes);

	FString GraphName;
	UEdGraph* Graph = FUeremcpBlueprintEpicBridge::ResolveGraph(Blueprint, Options.GraphId, GraphName);
	if (!Graph)
	{
		OutResult.Error = FString::Printf(
			TEXT("graph '%s' not found on blueprint"),
			Options.GraphId.IsEmpty() ? TEXT("EventGraph") : *Options.GraphId);
		return false;
	}

	if (Options.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.CapabilityNotes.Add(TEXT("submit_graph.dry_run_no_mutation"));
		return true;
	}

	FString WriteError;
	++OutResult.InternalOperations;
	if (!FUeremcpBlueprintEpicBridge::WriteGraphDsl(Graph, OutResult.DslUsed, WriteError))
	{
		OutResult.Error = WriteError;
		return false;
	}

	OutResult.bCompiled = Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
	if (Options.bCompile)
	{
		++OutResult.InternalOperations;
		if (Blueprint->Status == BS_Error)
		{
			OutResult.Error = TEXT("Blueprint compile finished with BS_Error after write_graph_dsl");
			return false;
		}
		if (!OutResult.bCompiled)
		{
			OutResult.Error = TEXT("Blueprint is not up to date after write_graph_dsl");
			return false;
		}
	}

	if (Options.bSave)
	{
		++OutResult.InternalOperations;
		if (UPackage* Package = Blueprint->GetOutermost())
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.Error = GWarn;
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(),
				FPackageName::GetAssetPackageExtension());
			const FSavePackageResultStruct SaveResult =
				UPackage::Save(Package, Blueprint, *Filename, SaveArgs);
			OutResult.bSaved = SaveResult.Result == ESavePackageResult::Success;
			if (!OutResult.bSaved)
			{
				OutResult.Error = TEXT("failed to save blueprint package after graph replace");
				return false;
			}
		}
	}

	FUeremcpBlueprintReadGraphOptions ReadOptions;
	ReadOptions.GraphId = Options.GraphId;
	ReadOptions.ResponseDetail = TEXT("complete");
	FUeremcpBlueprintReadGraphResult Reread;
	++OutResult.InternalOperations;
	if (!FUeremcpBlueprintGraphReader::ReadGraph(
			Blueprint,
			Options.AssetPath,
			ReadOptions,
			Reread))
	{
		OutResult.Error = Reread.Error.IsEmpty()
			? TEXT("failed to re-read graph after write")
			: Reread.Error;
		return false;
	}

	OutResult.RereadHash = Reread.ContentHash;
	OutResult.RevisionAfter = Reread.ContentHash;
	OutResult.RereadGraph = Reread.Graph;
	OutResult.bRereadAfterWrite = true;
	if (Options.ExpectedAfterWrite.IsValid())
	{
		OutResult.bExpectedStructureChecked = true;
		FString ExpectedError;
		OutResult.bExpectedStructureMatches =
			UeremcpBlueprintGraphWriter::ValidateExpectedAfterWrite(
				Reread.Graph,
				Options.ExpectedAfterWrite,
				ExpectedError);
		if (!OutResult.bExpectedStructureMatches)
		{
			OutResult.Error = ExpectedError;
			OutResult.CapabilityNotes.Add(TEXT("submit_graph.expected_after_write_mismatch"));
			return false;
		}
	}
	OutResult.bSuccess = true;
	return true;
}
