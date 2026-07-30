#include "UeremcpBlueprintGraphReader.h"

#include "UeremcpBlueprintEpicBridge.h"
#include "UeremcpContentHash.h"

#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"

namespace UeremcpBlueprintGraphReader
{
	static FString StripWhitespace(const FText& Text)
	{
		FString S = Text.ToString();
		S.ReplaceInline(TEXT(" "), TEXT(""));
		S.ReplaceInline(TEXT("\t"), TEXT(""));
		return S;
	}

	static FString ContainerTypeToString(const EPinContainerType Container)
	{
		switch (Container)
		{
		case EPinContainerType::Array: return TEXT("array");
		case EPinContainerType::Set: return TEXT("set");
		case EPinContainerType::Map: return TEXT("map");
		default: return TEXT("none");
		}
	}

	static TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
		if (!PinType.PinSubCategory.IsNone())
		{
			Obj->SetStringField(TEXT("sub_category"), PinType.PinSubCategory.ToString());
		}
		if (PinType.PinSubCategoryObject.IsValid())
		{
			Obj->SetStringField(TEXT("sub_category_object"), PinType.PinSubCategoryObject->GetPathName());
		}
		Obj->SetStringField(TEXT("container"), ContainerTypeToString(PinType.ContainerType));
		if (PinType.bIsReference)
		{
			Obj->SetBoolField(TEXT("is_reference"), true);
		}
		if (PinType.bIsConst)
		{
			Obj->SetBoolField(TEXT("is_const"), true);
		}
		return Obj;
	}

	static FString MakeNodeId(const UEdGraphNode* Node)
	{
		return FString::Printf(TEXT("n%s"), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	}

	static FString MakePinId(const UEdGraphPin* Pin)
	{
		return FString::Printf(TEXT("p%s"), *Pin->PinId.ToString(EGuidFormats::Digits));
	}

	static FString GetTypeId(const UEdGraphNode* Node)
	{
		if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
		{
			return FString::Printf(
				TEXT("AddEvent|Custom|%s"),
				*CustomEvent->CustomFunctionName.ToString());
		}

		if (const UK2Node* K2Node = Cast<UK2Node>(Node))
		{
			FString Title = StripWhitespace(K2Node->GetNodeTitle(ENodeTitleType::FullTitle));
			FString Category = StripWhitespace(K2Node->GetMenuCategory());
			if (Category.IsEmpty())
			{
				Category = TEXT("Unknown");
			}
			return FString::Printf(TEXT("%s|%s"), *Category, *Title);
		}

		return Node->GetClass()->GetName();
	}

	static FString EventEntryName(const UK2Node_Event* EventNode)
	{
		const FName MemberName = EventNode->EventReference.GetMemberName();
		FString EntryName = MemberName.ToString();
		if (EntryName.StartsWith(TEXT("Receive")))
		{
			EntryName = FString(TEXT("Event")) + EntryName.Mid(7);
		}
		return EntryName;
	}

	static FString GetSemanticType(const UEdGraphNode* Node)
	{
		if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
		{
			return FString::Printf(TEXT("custom_event:%s"), *CustomEvent->CustomFunctionName.ToString());
		}
		if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			return FString::Printf(TEXT("event:%s"), *EventEntryName(EventNode));
		}
		if (Node->IsA<UK2Node_FunctionEntry>())
		{
			return TEXT("function_entry");
		}
		if (Node->IsA<UK2Node_CallFunction>())
		{
			return TEXT("call_function");
		}
		return Node->GetClass()->GetName();
	}

	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	static bool IsEntryNode(const UEdGraphNode* Node)
	{
		return Node->IsA<UK2Node_Event>()
			|| Node->IsA<UK2Node_CustomEvent>()
			|| Node->IsA<UK2Node_FunctionEntry>();
	}

	static FString ResolveGraphType(UBlueprint* Blueprint, const UEdGraph* Graph)
	{
		if (!Blueprint || !Graph)
		{
			return TEXT("BlueprintEventGraph");
		}

		if (Graph->GetName().Equals(TEXT("UserConstructionScript"), ESearchCase::CaseSensitive))
		{
			return TEXT("BlueprintConstructionScript");
		}

		if (Blueprint->FunctionGraphs.Contains(Graph))
		{
			return TEXT("BlueprintFunctionGraph");
		}

		if (Blueprint->MacroGraphs.Contains(Graph))
		{
			return TEXT("BlueprintMacroGraph");
		}

		return TEXT("BlueprintEventGraph");
	}

	static UEdGraph* ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphId, FString& OutGraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		if (GraphId.IsEmpty())
		{
			UEdGraph* EventGraph = UBlueprintEditorLibrary::FindEventGraph(Blueprint);
			if (EventGraph)
			{
				OutGraphName = EventGraph->GetName();
			}
			return EventGraph;
		}

		const TArray<UEdGraph*> AllGraphs = UBlueprintEditorLibrary::ListGraphs(Blueprint);
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph && Graph->GetName().Equals(GraphId, ESearchCase::CaseSensitive))
			{
				OutGraphName = Graph->GetName();
				return Graph;
			}
		}

		return nullptr;
	}

	static TSharedPtr<FJsonValue> PinDefaultToJson(const UEdGraphPin* Pin)
	{
		if (!Pin->DefaultObject)
		{
			if (!Pin->DefaultValue.IsEmpty())
			{
				return MakeShared<FJsonValueString>(Pin->DefaultValue);
			}
			if (!Pin->AutogeneratedDefaultValue.IsEmpty())
			{
				return MakeShared<FJsonValueString>(Pin->AutogeneratedDefaultValue);
			}
			return nullptr;
		}

		return MakeShared<FJsonValueString>(Pin->DefaultObject->GetPathName());
	}

	static void CollectUpstreamPureNodes(
		const UEdGraphNode* Node,
		TSet<const UEdGraphNode*>& Visited,
		TArray<const UEdGraphNode*>& OutOrdered)
	{
		if (!Node || Visited.Contains(Node))
		{
			return;
		}
		Visited.Add(Node);

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked || !Linked->GetOwningNode())
				{
					continue;
				}

				const UEdGraphNode* Upstream = Linked->GetOwningNode();
				CollectUpstreamPureNodes(Upstream, Visited, OutOrdered);
			}
		}

		OutOrdered.Add(Node);
	}

	struct FEntryContext
	{
		FString Kind;
		FString Name;
		const UEdGraphNode* EntryNode = nullptr;
	};

	static TArray<FEntryContext> GatherEntryContexts(const UEdGraph* Graph)
	{
		TArray<FEntryContext> Entries;
		if (!Graph)
		{
			return Entries;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				FEntryContext Ctx;
				Ctx.Kind = TEXT("event");
				Ctx.Name = EventEntryName(EventNode);
				Ctx.EntryNode = EventNode;
				Entries.Add(MoveTemp(Ctx));
			}
			else if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				FEntryContext Ctx;
				Ctx.Kind = TEXT("event");
				Ctx.Name = CustomEvent->CustomFunctionName.ToString();
				Ctx.EntryNode = CustomEvent;
				Entries.Add(MoveTemp(Ctx));
			}
			else if (const UK2Node_FunctionEntry* FnEntry = Cast<UK2Node_FunctionEntry>(Node))
			{
				FEntryContext Ctx;
				Ctx.Kind = TEXT("fn");
				Ctx.Name = Graph->GetName();
				Ctx.EntryNode = FnEntry;
				Entries.Add(MoveTemp(Ctx));
			}
		}

		Entries.Sort([](const FEntryContext& A, const FEntryContext& B)
		{
			const FString KeyA = A.Kind + TEXT(":") + A.Name;
			const FString KeyB = B.Kind + TEXT(":") + B.Name;
			return KeyA < KeyB;
		});

		return Entries;
	}

	static void WalkExecFromEntry(
		const UEdGraphNode* Node,
		const FEntryContext& Entry,
		const FString& GraphName,
		TMap<const UEdGraphNode*, FString>& OutSemanticIds,
		TMap<const UEdGraphNode*, int32>& EntryTopoIndex,
		int32& InOutTopo)
	{
		if (!Node)
		{
			return;
		}
		if (OutSemanticIds.Contains(Node))
		{
			return;
		}

		TArray<const UEdGraphNode*> PureChain;
		TSet<const UEdGraphNode*> PureVisited;
		CollectUpstreamPureNodes(Node, PureVisited, PureChain);

		for (const UEdGraphNode* PureNode : PureChain)
		{
			if (OutSemanticIds.Contains(PureNode))
			{
				continue;
			}

			const FString TypeId = GetTypeId(PureNode);
			const int32 Topo = ++InOutTopo;
			EntryTopoIndex.Add(PureNode, Topo);
			const FString SemanticId = FString::Printf(
				TEXT("%s/%s:%s/n%03d:%s"),
				*GraphName,
				*Entry.Kind,
				*Entry.Name,
				Topo,
				*TypeId);
			OutSemanticIds.Add(PureNode, SemanticId);
		}

		if (!OutSemanticIds.Contains(Node))
		{
			const FString TypeId = GetTypeId(Node);
			const int32 Topo = ++InOutTopo;
			EntryTopoIndex.Add(Node, Topo);

			FString Qual;
			if (const UK2Node_CallFunction* CallFn = Cast<UK2Node_CallFunction>(Node))
			{
				if (const UFunction* Fn = CallFn->GetTargetFunction())
				{
					Qual = Fn->GetName();
				}
			}

			FString SemanticId = FString::Printf(
				TEXT("%s/%s:%s/n%03d:%s"),
				*GraphName,
				*Entry.Kind,
				*Entry.Name,
				Topo,
				*TypeId);
			if (!Qual.IsEmpty())
			{
				SemanticId += FString::Printf(TEXT("#%s"), *Qual);
			}
			OutSemanticIds.Add(Node, SemanticId);
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode())
				{
					WalkExecFromEntry(Linked->GetOwningNode(), Entry, GraphName, OutSemanticIds, EntryTopoIndex, InOutTopo);
				}
			}
		}
	}

	static void AssignSemanticIds(
		const UEdGraph* Graph,
		const FString& GraphName,
		TMap<const UEdGraphNode*, FString>& OutSemanticIds)
	{
		const TArray<FEntryContext> Entries = GatherEntryContexts(Graph);
		int32 OrphanTopo = 0;

		for (const FEntryContext& Entry : Entries)
		{
			int32 Topo = 0;
			TMap<const UEdGraphNode*, int32> EntryTopoIndex;
			WalkExecFromEntry(Entry.EntryNode, Entry, GraphName, OutSemanticIds, EntryTopoIndex, Topo);
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (OutSemanticIds.Contains(Node))
			{
				continue;
			}

			const FString TypeId = GetTypeId(Node);
			const FString SemanticId = FString::Printf(
				TEXT("%s/orphan/n%03d:%s"),
				*GraphName,
				++OrphanTopo,
				*TypeId);
			OutSemanticIds.Add(Node, SemanticId);
		}
	}

	static void ComputeReachability(
		const UEdGraph* Graph,
		TSet<const UEdGraphNode*>& OutReachable)
	{
		const TArray<FEntryContext> Entries = GatherEntryContexts(Graph);
		TArray<const UEdGraphNode*> Queue;

		for (const FEntryContext& Entry : Entries)
		{
			if (Entry.EntryNode)
			{
				Queue.Add(Entry.EntryNode);
			}
		}

		while (Queue.Num() > 0)
		{
			const UEdGraphNode* Node = Queue.Pop(EAllowShrinking::No);
			if (!Node || OutReachable.Contains(Node))
			{
				continue;
			}

			OutReachable.Add(Node);

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					continue;
				}

				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode())
					{
						Queue.Add(Linked->GetOwningNode());
					}
				}
			}
		}
	}

	static void FindDisconnectedSubgraphs(
		const UEdGraph* Graph,
		const TSet<const UEdGraphNode*>& Reachable,
		TArray<TArray<FString>>& OutSubgraphs,
		const TMap<const UEdGraphNode*, FString>& NodeIds)
	{
		TSet<const UEdGraphNode*> Unreachable;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Reachable.Contains(Node))
			{
				Unreachable.Add(Node);
			}
		}

		while (Unreachable.Num() > 0)
		{
			const UEdGraphNode* Seed = nullptr;
			for (const UEdGraphNode* Node : Unreachable)
			{
				Seed = Node;
				break;
			}

			if (!Seed)
			{
				break;
			}

			TSet<const UEdGraphNode*> Component;
			TArray<const UEdGraphNode*> Stack;
			Stack.Add(Seed);

			while (Stack.Num() > 0)
			{
				const UEdGraphNode* Node = Stack.Pop(EAllowShrinking::No);
				if (!Node || !Unreachable.Contains(Node) || Component.Contains(Node))
				{
					continue;
				}

				Component.Add(Node);
				Unreachable.Remove(Node);

				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin)
					{
						continue;
					}

					for (const UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (Linked && Linked->GetOwningNode())
						{
							Stack.Add(Linked->GetOwningNode());
						}
					}
				}
			}

			if (Component.Num() > 0)
			{
				TArray<FString> Island;
				Island.Reserve(Component.Num());
				for (const UEdGraphNode* Node : Component)
				{
					if (const FString* Id = NodeIds.Find(Node))
					{
						Island.Add(*Id);
					}
				}
				Island.Sort();
				if (Island.Num() > 0)
				{
					OutSubgraphs.Add(MoveTemp(Island));
				}
			}
		}
	}

	static bool ExecuteEpicToolSync(
		const FString& ToolName,
		const FString& JsonInput,
		FString& OutJsonResult,
		FString& OutError)
	{
		return FUeremcpBlueprintEpicBridge::ExecuteToolSync(ToolName, JsonInput, OutJsonResult, OutError);
	}

	static bool TryAttachDslExtension(
		const UEdGraph* Graph,
		TSharedPtr<FJsonObject>& Extensions,
		int32& InOutInternalOps,
		TArray<FString>& OutWarnings)
	{
		if (!Graph)
		{
			return false;
		}

		const FString RefPath = Graph->GetPathName();
		const FString Input = FString::Printf(
			TEXT("{\"graph\":{\"refPath\":\"%s\"}}"),
			*RefPath);

		FString ToolResult;
		FString ToolError;
		++InOutInternalOps;
		if (!ExecuteEpicToolSync(
				TEXT("read_graph_dsl"),
				Input,
				ToolResult,
				ToolError))
		{
			OutWarnings.Add(FString::Printf(TEXT("read_graph_dsl failed: %s"), *ToolError));
			return false;
		}

		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ToolResult);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			OutWarnings.Add(TEXT("read_graph_dsl returned non-JSON payload"));
			return false;
		}

		FString Dsl;
		if (!Parsed->TryGetStringField(TEXT("returnValue"), Dsl))
		{
			OutWarnings.Add(TEXT("read_graph_dsl missing returnValue"));
			return false;
		}

		TSharedPtr<FJsonObject> BlueprintExt = MakeShared<FJsonObject>();
		BlueprintExt->SetStringField(TEXT("dsl"), Dsl);
		Extensions->SetObjectField(TEXT("blueprint"), BlueprintExt);
		return true;
	}

	static TArray<TSharedPtr<FJsonValue>> SerializeVariables(const UBlueprint* Blueprint)
	{
		TArray<TSharedPtr<FJsonValue>> Vars;
		if (!Blueprint)
		{
			return Vars;
		}

		TArray<FBPVariableDescription> Sorted = Blueprint->NewVariables;
		Sorted.Sort([](const FBPVariableDescription& A, const FBPVariableDescription& B)
		{
			return A.VarName.LexicalLess(B.VarName);
		});

		for (const FBPVariableDescription& Var : Sorted)
		{
			TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
			VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());

			TSharedPtr<FJsonObject> TypeObj = PinTypeToJson(Var.VarType);
			VarObj->SetObjectField(TEXT("type"), TypeObj);

			if (!Var.DefaultValue.IsEmpty())
			{
				VarObj->SetStringField(TEXT("default_value"), Var.DefaultValue);
			}

			const bool bReplicated = (Var.PropertyFlags & CPF_Net) != 0;
			if (bReplicated)
			{
				VarObj->SetBoolField(TEXT("is_replicated"), true);
			}

			if (!Var.Category.IsEmpty())
			{
				VarObj->SetStringField(TEXT("category"), Var.Category.ToString());
			}

			Vars.Add(MakeShared<FJsonValueObject>(VarObj));
		}

		return Vars;
	}

	static FString FormatEngineVersionString()
	{
		const FEngineVersion Ver = FEngineVersion::Current();
		return FString::Printf(TEXT("%u.%u.%u"), Ver.GetMajor(), Ver.GetMinor(), Ver.GetPatch());
	}

	static TSharedPtr<FJsonObject> MakeSemanticHashProjection(const TSharedPtr<FJsonObject>& Graph)
	{
		TSharedPtr<FJsonObject> Projection = MakeShared<FJsonObject>();
		for (const TCHAR* Field : {
				TEXT("schema_version"),
				TEXT("graph_type"),
				TEXT("graph_name"),
				TEXT("nodes"),
				TEXT("links"),
				TEXT("variables"),
				TEXT("functions"),
				TEXT("macros"),
				TEXT("events"),
				TEXT("subgraphs"),
				TEXT("comments")})
		{
			if (Graph->HasField(Field))
			{
				Projection->SetField(Field, Graph->TryGetField(Field));
			}
		}
		return Projection;
	}
}

using namespace UeremcpBlueprintGraphReader;

TArray<FString> FUeremcpBlueprintGraphReader::DefaultLossyAreas()
{
	return {
		TEXT("multigate_no_dsl_roundtrip"),
		TEXT("timeline_special_spawn"),
		TEXT("math_expression_unproven"),
		TEXT("dsl_bind_elision"),
		TEXT("reroute_knots_elided"),
		TEXT("node_guid_not_preserved"),
		TEXT("positions_not_semantic"),
		TEXT("collapsed_composite_subgraphs_unproven"),
		TEXT("project_custom_k2_nodes_unknown"),
	};
}

FString FUeremcpBlueprintGraphReader::ComputeContentHash(
	const TSharedPtr<FJsonObject>& Graph,
	FString* OutError)
{
	if (!Graph.IsValid())
	{
		if (OutError)
		{
			*OutError = TEXT("graph is null");
		}
		return FString();
	}
	return FUeremcpContentHash::HashJsonObject(MakeSemanticHashProjection(Graph), OutError);
}

bool FUeremcpBlueprintGraphReader::ReadGraph(
	UBlueprint* Blueprint,
	const FString& AssetPath,
	const FUeremcpBlueprintReadGraphOptions& Options,
	FUeremcpBlueprintReadGraphResult& OutResult)
{
	OutResult = FUeremcpBlueprintReadGraphResult();

	if (!Blueprint)
	{
		OutResult.Error = TEXT("Blueprint is null");
		return false;
	}

	FString GraphName;
	UEdGraph* Graph = ResolveTargetGraph(Blueprint, Options.GraphId, GraphName);
	if (!Graph)
	{
		OutResult.Error = FString::Printf(
			TEXT("Graph '%s' not found on Blueprint '%s'"),
			Options.GraphId.IsEmpty() ? TEXT("EventGraph") : *Options.GraphId,
			*AssetPath);
		return false;
	}

	TMap<const UEdGraphNode*, FString> SemanticIds;
	AssignSemanticIds(Graph, GraphName, SemanticIds);

	TMap<const UEdGraphNode*, FString> NodeIds;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		NodeIds.Add(Node, MakeNodeId(Node));
	}

	TArray<TSharedPtr<FJsonValue>> NodeArray;
	TArray<TSharedPtr<FJsonValue>> LinkArray;
	TSet<FString> DependencyPaths;

	const bool bIncludeFullNodes =
		Options.ResponseDetail.Equals(TEXT("complete"), ESearchCase::IgnoreCase)
		|| Options.ResponseDetail.Equals(TEXT("diagnostic"), ESearchCase::IgnoreCase);

	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			const FString& NodeId = NodeIds[Node];
			NodeObj->SetStringField(TEXT("node_id"), NodeId);
			if (const FString* SemanticId = SemanticIds.Find(Node))
			{
				NodeObj->SetStringField(TEXT("semantic_id"), *SemanticId);
			}

			NodeObj->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("semantic_type"), GetSemanticType(Node));
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			{
				TArray<TSharedPtr<FJsonValue>> Pos;
				Pos.Add(MakeShared<FJsonValueNumber>(Node->NodePosX));
				Pos.Add(MakeShared<FJsonValueNumber>(Node->NodePosY));
				NodeObj->SetArrayField(TEXT("position"), Pos);
			}

			if (const UK2Node_CallFunction* CallFn = Cast<UK2Node_CallFunction>(Node))
			{
				TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
				Props->SetStringField(TEXT("type_id"), GetTypeId(Node));
				if (const UFunction* Fn = CallFn->GetTargetFunction())
				{
					Props->SetStringField(TEXT("function"), Fn->GetPathName());
				}
				NodeObj->SetObjectField(TEXT("properties"), Props);
			}
			else
			{
				TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
				Props->SetStringField(TEXT("type_id"), GetTypeId(Node));
				NodeObj->SetObjectField(TEXT("properties"), Props);
			}

			if (!Node->NodeComment.IsEmpty())
			{
				NodeObj->SetStringField(TEXT("comment"), Node->NodeComment);
			}

			TArray<TSharedPtr<FJsonValue>> InputPins;
			TArray<TSharedPtr<FJsonValue>> OutputPins;
			TSharedPtr<FJsonObject> Defaults = MakeShared<FJsonObject>();

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					continue;
				}

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("pin_id"), MakePinId(Pin));
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(
					TEXT("direction"),
					Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
				PinObj->SetObjectField(TEXT("pin_type"), PinTypeToJson(Pin->PinType));

				if (Pin->PinType.PinSubCategoryObject.IsValid())
				{
					DependencyPaths.Add(Pin->PinType.PinSubCategoryObject->GetPathName());
				}

				if (TSharedPtr<FJsonValue> DefaultVal = PinDefaultToJson(Pin))
				{
					Defaults->SetField(Pin->PinName.ToString(), DefaultVal);
					PinObj->SetField(TEXT("default_value"), DefaultVal);
				}

				TArray<TSharedPtr<FJsonValue>> PinLinks;
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked || !Linked->GetOwningNode())
					{
						continue;
					}

					const UEdGraphNode* OtherNode = Linked->GetOwningNode();
					TSharedPtr<FJsonObject> LinkRef = MakeShared<FJsonObject>();
					LinkRef->SetStringField(TEXT("node_id"), NodeIds[OtherNode]);
					LinkRef->SetStringField(TEXT("pin_id"), MakePinId(Linked));
					PinLinks.Add(MakeShared<FJsonValueObject>(LinkRef));

					if (Pin->Direction == EGPD_Output)
					{
						TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
						Edge->SetStringField(TEXT("from_node"), NodeId);
						Edge->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
						Edge->SetStringField(TEXT("to_node"), NodeIds[OtherNode]);
						Edge->SetStringField(TEXT("to_pin"), Linked->PinName.ToString());
						Edge->SetStringField(TEXT("kind"), IsExecPin(Pin) ? TEXT("exec") : TEXT("data"));
						Edge->SetBoolField(TEXT("valid"), true);
						LinkArray.Add(MakeShared<FJsonValueObject>(Edge));
					}
				}

				if (PinLinks.Num() > 0)
				{
					PinObj->SetArrayField(TEXT("links"), PinLinks);
				}

				if (Pin->Direction == EGPD_Input)
				{
					InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
				}
				else
				{
					OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
				}
			}

			if (Defaults->Values.Num() > 0)
			{
				NodeObj->SetObjectField(TEXT("defaults"), Defaults);
			}
			NodeObj->SetArrayField(TEXT("input_pins"), InputPins);
			NodeObj->SetArrayField(TEXT("output_pins"), OutputPins);

			NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}
	}

	TSet<const UEdGraphNode*> Reachable;
	ComputeReachability(Graph, Reachable);

	TArray<FString> DeadNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Reachable.Contains(Node) && !IsEntryNode(Node))
		{
			if (const FString* Id = NodeIds.Find(Node))
			{
				DeadNodes.Add(*Id);
			}
		}
	}
	DeadNodes.Sort();

	TArray<TArray<FString>> DisconnectedSubgraphs;
	FindDisconnectedSubgraphs(Graph, Reachable, DisconnectedSubgraphs, NodeIds);

	TArray<FString> EntryPoints;
	for (const FEntryContext& Entry : GatherEntryContexts(Graph))
	{
		if (const FString* Id = NodeIds.Find(Entry.EntryNode))
		{
			EntryPoints.Add(*Id);
		}
	}

	TArray<TSharedPtr<FJsonValue>> CompilerErrors;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node->ErrorMsg.IsEmpty())
		{
			TSharedPtr<FJsonObject> Diag = MakeShared<FJsonObject>();
			Diag->SetStringField(TEXT("severity"), TEXT("error"));
			Diag->SetStringField(TEXT("code"), TEXT("blueprint.node_error"));
			Diag->SetStringField(TEXT("message"), Node->ErrorMsg);
			if (const FString* Id = NodeIds.Find(Node))
			{
				Diag->SetStringField(TEXT("node_id"), *Id);
			}
			CompilerErrors.Add(MakeShared<FJsonValueObject>(Diag));
		}
	}

	TSharedPtr<FJsonObject> GraphDiagnostics = MakeShared<FJsonObject>();
	if (DeadNodes.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Dead;
		for (const FString& Id : DeadNodes)
		{
			Dead.Add(MakeShared<FJsonValueString>(Id));
		}
		GraphDiagnostics->SetArrayField(TEXT("dead_nodes"), Dead);
	}

	if (DisconnectedSubgraphs.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Islands;
		for (const TArray<FString>& Island : DisconnectedSubgraphs)
		{
			TArray<TSharedPtr<FJsonValue>> IslandJson;
			for (const FString& Id : Island)
			{
				IslandJson.Add(MakeShared<FJsonValueString>(Id));
			}
			Islands.Add(MakeShared<FJsonValueArray>(IslandJson));
		}
		GraphDiagnostics->SetArrayField(TEXT("disconnected_subgraphs"), Islands);
	}

	if (CompilerErrors.Num() > 0)
	{
		GraphDiagnostics->SetArrayField(TEXT("errors"), CompilerErrors);
	}

	TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
	GraphObj->SetStringField(TEXT("asset_path"), AssetPath);
	GraphObj->SetStringField(TEXT("graph_id"), GraphName);
	GraphObj->SetStringField(TEXT("graph_name"), GraphName);
	GraphObj->SetStringField(TEXT("graph_type"), ResolveGraphType(Blueprint, Graph));
	GraphObj->SetStringField(TEXT("schema_version"), TEXT("1.0"));
	GraphObj->SetStringField(TEXT("engine_version"), FormatEngineVersionString());
	GraphObj->SetStringField(TEXT("retrieved_at"), FDateTime::UtcNow().ToIso8601());

	TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
	Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
	TArray<TSharedPtr<FJsonValue>> Lossy;
	for (const FString& Area : DefaultLossyAreas())
	{
		Lossy.Add(MakeShared<FJsonValueString>(Area));
	}
	Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
	GraphObj->SetObjectField(TEXT("fidelity"), Fidelity);

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetStringField(TEXT("purpose"), FString::Printf(TEXT("Blueprint graph '%s'"), *GraphName));
	Summary->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
	GraphObj->SetObjectField(TEXT("semantic_summary"), Summary);

	if (EntryPoints.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> EntryJson;
		for (const FString& Ep : EntryPoints)
		{
			EntryJson.Add(MakeShared<FJsonValueString>(Ep));
		}
		GraphObj->SetArrayField(TEXT("entry_points"), EntryJson);
	}

	if (bIncludeFullNodes)
	{
		GraphObj->SetArrayField(TEXT("nodes"), NodeArray);
		GraphObj->SetArrayField(TEXT("links"), LinkArray);
	}

	const TArray<TSharedPtr<FJsonValue>> Variables = SerializeVariables(Blueprint);
	if (Variables.Num() > 0)
	{
		GraphObj->SetArrayField(TEXT("variables"), Variables);
	}

	if (DependencyPaths.Num() > 0)
	{
		TArray<FString> SortedDeps = DependencyPaths.Array();
		SortedDeps.Sort();
		TArray<TSharedPtr<FJsonValue>> DepJson;
		for (const FString& Dep : SortedDeps)
		{
			TSharedPtr<FJsonObject> Ref = MakeShared<FJsonObject>();
			Ref->SetStringField(TEXT("asset_path"), Dep);
			DepJson.Add(MakeShared<FJsonValueObject>(Ref));
		}
		GraphObj->SetArrayField(TEXT("dependencies"), DepJson);
	}

	GraphObj->SetObjectField(TEXT("diagnostics"), GraphDiagnostics);

	TSharedPtr<FJsonObject> Extensions = MakeShared<FJsonObject>();
	TArray<FString> DslWarnings;
	if (Options.bIncludeDsl)
	{
		TryAttachDslExtension(Graph, Extensions, OutResult.InternalOperations, DslWarnings);
	}
	if (DslWarnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Warnings;
		for (const FString& Warning : DslWarnings)
		{
			TSharedPtr<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
			Diagnostic->SetStringField(TEXT("code"), TEXT("blueprint.dsl_extension"));
			Diagnostic->SetStringField(TEXT("message"), Warning);
			Warnings.Add(MakeShared<FJsonValueObject>(Diagnostic));
		}
		GraphDiagnostics->SetArrayField(TEXT("warnings"), Warnings);
	}
	if (Extensions->Values.Num() > 0)
	{
		GraphObj->SetObjectField(TEXT("extensions"), Extensions);
	}

	FString HashError;
	const FString ContentHash = ComputeContentHash(GraphObj, &HashError);
	if (ContentHash.IsEmpty())
	{
		OutResult.Error = FString::Printf(TEXT("content_hash failed: %s"), *HashError);
		return false;
	}

	GraphObj->SetStringField(TEXT("content_hash"), ContentHash);
	GraphObj->SetStringField(TEXT("revision"), ContentHash);

	if (!bIncludeFullNodes)
	{
		GraphObj->RemoveField(TEXT("nodes"));
		GraphObj->RemoveField(TEXT("links"));
		Fidelity->SetBoolField(TEXT("omitted_for_size"), true);
	}

	OutResult.Graph = GraphObj;
	OutResult.ContentHash = ContentHash;
	OutResult.bSuccess = true;
	return true;
}
