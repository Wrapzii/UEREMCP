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
			(*Defaults)->Values.GetKeys(DefaultKeys);
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
}

bool FUeremcpBlueprintGraphWriter::IsScratchAssetPath(const FString& AssetPath)
{
	return AssetPath.StartsWith(TEXT("/Game/__UeremcpTests/"), ESearchCase::CaseSensitive);
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
		OutResult.Error = TEXT("submit_graph replace writes are restricted to /Game/__UeremcpTests/ scratch assets");
		OutResult.CapabilityNotes.Add(TEXT("submit_graph.scratch_path_only"));
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
	OutResult.bSuccess = true;
	return true;
}
