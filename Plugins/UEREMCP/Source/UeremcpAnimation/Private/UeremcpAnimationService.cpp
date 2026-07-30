#include "UeremcpAnimationService.h"

#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "UeremcpContentHash.h"
#include "UeremcpEdGraphReader.h"

namespace
{
	TSharedPtr<FJsonValue> JsonObjectValue(const TSharedPtr<FJsonObject>& Object)
	{
		return MakeShared<FJsonValueObject>(Object);
	}

	FString ObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	FName NotifyTrackName(const UAnimMontage& Montage, const FAnimNotifyEvent& Event)
	{
#if WITH_EDITORONLY_DATA
		if (Montage.AnimNotifyTracks.IsValidIndex(Event.TrackIndex))
		{
			return Montage.AnimNotifyTracks[Event.TrackIndex].TrackName;
		}
#endif
		return NAME_None;
	}

	FString ClassifyAnimBlueprintGraph(const UEdGraph* Graph)
	{
		if (Cast<UAnimationStateMachineGraph>(Graph))
		{
			return TEXT("AnimStateMachine");
		}
		return TEXT("AnimBlueprintGraph");
	}

	FString GraphId(const UEdGraph* Graph)
	{
		return Graph ? Graph->GetName() : FString();
	}

	FString AnimSemanticType(const UEdGraphNode* Node)
	{
		if (Cast<UAnimStateEntryNode>(Node))
		{
			return TEXT("anim_state_entry");
		}
		if (Cast<UAnimStateTransitionNode>(Node))
		{
			return TEXT("anim_state_transition");
		}
		if (Cast<UAnimStateNode>(Node))
		{
			return TEXT("anim_state");
		}
		if (Cast<UAnimGraphNode_StateMachineBase>(Node))
		{
			return TEXT("anim_state_machine");
		}
		if (Cast<UAnimGraphNode_Base>(Node))
		{
			return TEXT("anim_graph_node");
		}
		return Node ? Node->GetClass()->GetName() : TEXT("unknown");
	}

	FString AnimSemanticId(const UEdGraphNode* Node)
	{
		if (const UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			// [VERIFIED: AnimStateNode.h:67-71]
			return FString::Printf(TEXT("state:%s"), *State->GetStateName());
		}
		if (const UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node))
		{
			// [VERIFIED: AnimStateTransitionNode.h:172-177]
			const UAnimStateNodeBase* Previous = Transition->GetPreviousState();
			const UAnimStateNodeBase* Next = Transition->GetNextState();
			return FString::Printf(
				TEXT("transition:%s->%s"),
				Previous ? *Previous->GetStateName() : TEXT("?"),
				Next ? *Next->GetStateName() : TEXT("?"));
		}
		if (Cast<UAnimStateEntryNode>(Node))
		{
			return TEXT("entry");
		}
		if (UAnimGraphNode_StateMachineBase* StateMachine =
			Cast<UAnimGraphNode_StateMachineBase>(const_cast<UEdGraphNode*>(Node)))
		{
			// [VERIFIED: AnimGraphNode_StateMachineBase.h:47-51]
			return FString::Printf(TEXT("state_machine:%s"), *StateMachine->GetStateMachineName());
		}
		return FString();
	}

	TSharedPtr<FJsonObject> AnimNodeProperties(const UEdGraphNode* Node)
	{
		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetStringField(TEXT("type_id"), Node->GetClass()->GetPathName());

		if (const UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			// [VERIFIED: AnimStateNode.h:29-48,67-81]
			Properties->SetStringField(TEXT("state_name"), State->GetStateName());
			Properties->SetStringField(TEXT("bound_graph_id"), GraphId(State->GetBoundGraph()));
			Properties->SetBoolField(TEXT("always_reset_on_entry"), State->bAlwaysResetOnEntry);
		}
		else if (const UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node))
		{
			// [VERIFIED: AnimStateTransitionNode.h:24-82,165-177]
			const UAnimStateNodeBase* Previous = Transition->GetPreviousState();
			const UAnimStateNodeBase* Next = Transition->GetNextState();
			Properties->SetStringField(
				TEXT("from_state"), Previous ? Previous->GetStateName() : FString());
			Properties->SetStringField(
				TEXT("to_state"), Next ? Next->GetStateName() : FString());
			Properties->SetStringField(TEXT("rule_graph_id"), GraphId(Transition->GetBoundGraph()));
			Properties->SetNumberField(TEXT("priority_order"), Transition->PriorityOrder);
			Properties->SetNumberField(TEXT("crossfade_duration"), Transition->CrossfadeDuration);
			Properties->SetBoolField(
				TEXT("automatic_rule"),
				Transition->bAutomaticRuleBasedOnSequencePlayerInState);
		}
		else if (const UAnimStateEntryNode* Entry = Cast<UAnimStateEntryNode>(Node))
		{
			// [VERIFIED: AnimStateEntryNode.h:29-30]
			const UEdGraphNode* Output = Entry->GetOutputNode();
			Properties->SetStringField(
				TEXT("entry_state"),
				Output ? Output->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString());
		}
		else if (UAnimGraphNode_StateMachineBase* StateMachine =
			Cast<UAnimGraphNode_StateMachineBase>(const_cast<UEdGraphNode*>(Node)))
		{
			// [VERIFIED: AnimGraphNode_StateMachineBase.h:21-23,47-51]
			Properties->SetStringField(TEXT("state_machine_name"), StateMachine->GetStateMachineName());
			Properties->SetStringField(
				TEXT("state_machine_graph_id"),
				GraphId(StateMachine->EditorStateMachineGraph));
		}

		return Properties;
	}

	TSharedPtr<FJsonObject> BuildAnimationExtension(
		const UAnimBlueprint* AnimBlueprint,
		const TArray<UEdGraph*>& AllGraphs)
	{
		TSharedPtr<FJsonObject> Animation = MakeShared<FJsonObject>();
		const FString SkeletonPath = ObjectPath(AnimBlueprint->TargetSkeleton.Get());
		if (!SkeletonPath.IsEmpty())
		{
			Animation->SetStringField(TEXT("skeleton"), SkeletonPath);
		}

		// [VERIFIED: AnimBlueprint.h:202-205]
		const FString PreviewMeshPath = ObjectPath(AnimBlueprint->GetPreviewMesh());
		if (!PreviewMeshPath.IsEmpty())
		{
			Animation->SetStringField(TEXT("preview_mesh"), PreviewMeshPath);
		}

		TArray<TSharedPtr<FJsonValue>> StateMachines;
		for (UEdGraph* Candidate : AllGraphs)
		{
			const UAnimationStateMachineGraph* StateMachine =
				Cast<UAnimationStateMachineGraph>(Candidate);
			if (!StateMachine)
			{
				continue;
			}

			TSharedPtr<FJsonObject> StateMachineJson = MakeShared<FJsonObject>();
			StateMachineJson->SetStringField(TEXT("name"), StateMachine->GetName());

			// [VERIFIED: AnimationStateMachineGraph.h:20-26]
			if (StateMachine->EntryNode)
			{
				if (const UAnimStateNodeBase* EntryState =
					Cast<UAnimStateNodeBase>(StateMachine->EntryNode->GetOutputNode()))
				{
					StateMachineJson->SetStringField(TEXT("entry_state"), EntryState->GetStateName());
				}
			}

			TArray<TSharedPtr<FJsonValue>> States;
			TArray<TSharedPtr<FJsonValue>> Transitions;
			for (UEdGraphNode* Node : StateMachine->Nodes)
			{
				if (const UAnimStateNode* State = Cast<UAnimStateNode>(Node))
				{
					TSharedPtr<FJsonObject> StateJson = MakeShared<FJsonObject>();
					StateJson->SetStringField(TEXT("name"), State->GetStateName());
					StateJson->SetStringField(TEXT("bound_graph_id"), GraphId(State->GetBoundGraph()));
					States.Add(JsonObjectValue(StateJson));
				}
				else if (const UAnimStateTransitionNode* Transition =
					Cast<UAnimStateTransitionNode>(Node))
				{
					const UAnimStateNodeBase* Previous = Transition->GetPreviousState();
					const UAnimStateNodeBase* Next = Transition->GetNextState();
					TSharedPtr<FJsonObject> TransitionJson = MakeShared<FJsonObject>();
					TransitionJson->SetStringField(
						TEXT("from"), Previous ? Previous->GetStateName() : FString());
					TransitionJson->SetStringField(
						TEXT("to"), Next ? Next->GetStateName() : FString());
					TransitionJson->SetStringField(
						TEXT("rule_graph_id"), GraphId(Transition->GetBoundGraph()));
					Transitions.Add(JsonObjectValue(TransitionJson));
				}
			}

			States.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
			{
				return A->AsObject()->GetStringField(TEXT("name"))
					< B->AsObject()->GetStringField(TEXT("name"));
			});
			Transitions.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
			{
				const TSharedPtr<FJsonObject> AObject = A->AsObject();
				const TSharedPtr<FJsonObject> BObject = B->AsObject();
				const FString AKey = AObject->GetStringField(TEXT("from"))
					+ TEXT("->") + AObject->GetStringField(TEXT("to"));
				const FString BKey = BObject->GetStringField(TEXT("from"))
					+ TEXT("->") + BObject->GetStringField(TEXT("to"));
				return AKey < BKey;
			});
			StateMachineJson->SetArrayField(TEXT("states"), States);
			StateMachineJson->SetArrayField(TEXT("transitions"), Transitions);
			StateMachines.Add(JsonObjectValue(StateMachineJson));
		}
		StateMachines.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
		{
			return A->AsObject()->GetStringField(TEXT("name"))
				< B->AsObject()->GetStringField(TEXT("name"));
		});
		Animation->SetArrayField(TEXT("state_machines"), StateMachines);
		return Animation;
	}
}

bool FUeremcpAnimationService::InspectMontage(
	const UAnimMontage* Montage,
	const FString& AssetPath,
	FUeremcpMontageInspection& OutInspection,
	FString& OutError)
{
	OutInspection = FUeremcpMontageInspection();
	OutError.Reset();

	if (!Montage)
	{
		OutError = TEXT("Montage is null.");
		return false;
	}

	TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
	State->SetStringField(TEXT("asset_path"), AssetPath);
	State->SetStringField(TEXT("asset_class"), Montage->GetClass()->GetPathName());
	State->SetNumberField(TEXT("length_seconds"), Montage->GetPlayLength());
	State->SetBoolField(TEXT("auto_blend_out"), Montage->bEnableAutoBlendOut);
	State->SetStringField(TEXT("sync_group"), Montage->SyncGroup.ToString());

	if (const USkeleton* Skeleton = Montage->GetSkeleton())
	{
		const FString SkeletonPath = Skeleton->GetPathName();
		State->SetStringField(TEXT("skeleton"), SkeletonPath);
		OutInspection.DependencyPaths.AddUnique(SkeletonPath);
	}
	else
	{
		State->SetField(TEXT("skeleton"), MakeShared<FJsonValueNull>());
	}

	TArray<TSharedPtr<FJsonValue>> Slots;
	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		TSharedPtr<FJsonObject> SlotObject = MakeShared<FJsonObject>();
		SlotObject->SetStringField(TEXT("name"), Slot.SlotName.ToString());

		TArray<TSharedPtr<FJsonValue>> Segments;
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			TSharedPtr<FJsonObject> SegmentObject = MakeShared<FJsonObject>();
			const UAnimSequenceBase* Animation = Segment.GetAnimReference();
			const FString AnimationPath = ObjectPath(Animation);
			if (!AnimationPath.IsEmpty())
			{
				SegmentObject->SetStringField(TEXT("animation"), AnimationPath);
				OutInspection.DependencyPaths.AddUnique(AnimationPath);
			}
			else
			{
				SegmentObject->SetField(TEXT("animation"), MakeShared<FJsonValueNull>());
			}
			SegmentObject->SetNumberField(TEXT("start_time"), Segment.StartPos);
			SegmentObject->SetNumberField(TEXT("animation_start_time"), Segment.AnimStartTime);
			SegmentObject->SetNumberField(TEXT("animation_end_time"), Segment.AnimEndTime);
			SegmentObject->SetNumberField(TEXT("play_rate"), Segment.AnimPlayRate);
			SegmentObject->SetNumberField(TEXT("loop_count"), Segment.LoopingCount);
			Segments.Add(JsonObjectValue(SegmentObject));
			++OutInspection.SegmentCount;
		}

		SlotObject->SetArrayField(TEXT("segments"), Segments);
		Slots.Add(JsonObjectValue(SlotObject));
	}
	State->SetArrayField(TEXT("slots"), Slots);
	OutInspection.SlotCount = Slots.Num();

	TArray<TSharedPtr<FJsonValue>> Sections;
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		TSharedPtr<FJsonObject> SectionObject = MakeShared<FJsonObject>();
		SectionObject->SetStringField(TEXT("name"), Section.SectionName.ToString());
		SectionObject->SetNumberField(TEXT("start_time"), Section.GetTime());
		if (!Section.NextSectionName.IsNone())
		{
			SectionObject->SetStringField(TEXT("next_section"), Section.NextSectionName.ToString());
		}
		Sections.Add(JsonObjectValue(SectionObject));
	}
	State->SetArrayField(TEXT("sections"), Sections);
	OutInspection.SectionCount = Sections.Num();

	TArray<FAnimNotifyEvent> NotifyEvents;
	// Public read API; avoids treating REAgentTools notify-plan metadata as real notifies.
	// [VERIFIED: AnimationBlueprintLibrary.h:230-232]
	UAnimationBlueprintLibrary::GetAnimationNotifyEvents(Montage, NotifyEvents);
	// Match the engine's canonical notify ordering: trigger time, then track index.
	// [VERIFIED: Runtime/Engine/Public/Animation/AnimTypes.h:458-474]
	NotifyEvents.Sort();

	TArray<TSharedPtr<FJsonValue>> Notifies;
	for (const FAnimNotifyEvent& Event : NotifyEvents)
	{
		TSharedPtr<FJsonObject> NotifyObject = MakeShared<FJsonObject>();
		NotifyObject->SetStringField(TEXT("name"), Event.NotifyName.ToString());
		NotifyObject->SetNumberField(TEXT("time"), Event.GetTriggerTime());
		NotifyObject->SetNumberField(TEXT("duration"), Event.GetDuration());
		NotifyObject->SetStringField(TEXT("track"), NotifyTrackName(*Montage, Event).ToString());
		NotifyObject->SetNumberField(TEXT("track_index"), Event.TrackIndex);
		NotifyObject->SetNumberField(TEXT("trigger_chance"), Event.NotifyTriggerChance);
		NotifyObject->SetBoolField(TEXT("trigger_on_dedicated_server"), Event.bTriggerOnDedicatedServer);

		const UObject* NotifyInstance = Event.Notify
			? static_cast<const UObject*>(Event.Notify.Get())
			: static_cast<const UObject*>(Event.NotifyStateClass.Get());
		NotifyObject->SetBoolField(TEXT("is_state"), Event.NotifyStateClass != nullptr);
		if (NotifyInstance)
		{
			NotifyObject->SetStringField(TEXT("class"), NotifyInstance->GetClass()->GetPathName());
		}
		else
		{
			NotifyObject->SetField(TEXT("class"), MakeShared<FJsonValueNull>());
		}

		Notifies.Add(JsonObjectValue(NotifyObject));
	}
	State->SetArrayField(TEXT("notifies"), Notifies);
	OutInspection.NotifyCount = Notifies.Num();

	OutInspection.ContentHash = FUeremcpContentHash::HashJsonObject(State, &OutError);
	if (OutInspection.ContentHash.IsEmpty())
	{
		return false;
	}

	State->SetStringField(TEXT("content_hash"), OutInspection.ContentHash);
	State->SetStringField(TEXT("revision"), OutInspection.ContentHash);
	OutInspection.State = MoveTemp(State);
	return true;
}

bool FUeremcpAnimationService::InspectAnimBlueprint(
	UAnimBlueprint* AnimBlueprint,
	const FString& AssetPath,
	FUeremcpAnimBlueprintInspection& OutInspection,
	FString& OutError)
{
	OutInspection = FUeremcpAnimBlueprintInspection();
	OutError.Reset();

	if (!AnimBlueprint)
	{
		OutError = TEXT("AnimBlueprint is null.");
		return false;
	}

	TSharedPtr<FJsonObject> Inventory = MakeShared<FJsonObject>();
	Inventory->SetStringField(TEXT("asset_path"), AssetPath);
	Inventory->SetStringField(TEXT("asset_class"), AnimBlueprint->GetClass()->GetPathName());
	Inventory->SetBoolField(TEXT("is_template"), AnimBlueprint->bIsTemplate);
	Inventory->SetStringField(TEXT("skeleton"), ObjectPath(AnimBlueprint->TargetSkeleton.Get()));

	if (AnimBlueprint->TargetSkeleton)
	{
		OutInspection.DependencyPaths.AddUnique(ObjectPath(AnimBlueprint->TargetSkeleton.Get()));
	}

	TArray<UEdGraph*> AllGraphs;
	AnimBlueprint->GetAllGraphs(AllGraphs);
	const TSharedPtr<FJsonObject> AnimationExtension =
		BuildAnimationExtension(AnimBlueprint, AllGraphs);

	struct FGraphSortKey
	{
		FString Name;
		FString Class;
		FString Type;
		int32 NodeCount = 0;
		UEdGraph* Graph = nullptr;
	};
	TArray<FGraphSortKey> Sorted;
	Sorted.Reserve(AllGraphs.Num());
	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}
		FGraphSortKey Key;
		Key.Name = Graph->GetName();
		Key.Class = Graph->GetClass()->GetPathName();
		Key.Type = ClassifyAnimBlueprintGraph(Graph);
		Key.NodeCount = Graph->Nodes.Num();
		Key.Graph = Graph;
		Sorted.Add(MoveTemp(Key));
	}
	Sorted.Sort([](const FGraphSortKey& A, const FGraphSortKey& B)
	{
		const int32 NameCmp = A.Name.Compare(B.Name);
		if (NameCmp != 0)
		{
			return NameCmp < 0;
		}
		const int32 ClassCmp = A.Class.Compare(B.Class);
		if (ClassCmp != 0)
		{
			return ClassCmp < 0;
		}
		const int32 TypeCmp = A.Type.Compare(B.Type);
		if (TypeCmp != 0)
		{
			return TypeCmp < 0;
		}
		return A.NodeCount < B.NodeCount;
	});

	TArray<TSharedPtr<FJsonValue>> GraphsJson;
	for (const FGraphSortKey& Entry : Sorted)
	{
		UEdGraph* Graph = Entry.Graph;
		const FString& GraphType = Entry.Type;
		const int32 NodeCount = Entry.NodeCount;

		FUeremcpEdGraphSemanticHooks Hooks;
		Hooks.ResolveSemanticType = AnimSemanticType;
		Hooks.ResolveSemanticId = AnimSemanticId;
		Hooks.ResolveProperties = AnimNodeProperties;
		Hooks.IsEntryNode = [](const UEdGraphNode* Node)
		{
			return Cast<UAnimStateEntryNode>(Node) != nullptr;
		};
		Hooks.GatherEntryNodes = [](const UEdGraph* Candidate)
		{
			TArray<const UEdGraphNode*> EntryNodes;
			if (const UAnimationStateMachineGraph* StateMachine =
				Cast<UAnimationStateMachineGraph>(Candidate))
			{
				if (StateMachine->EntryNode)
				{
					EntryNodes.Add(StateMachine->EntryNode);
				}
			}
			return EntryNodes;
		};
		Hooks.IsExecPin = [](const UEdGraphPin*)
		{
			// Anim pose/state-machine edges are dataflow, not Blueprint execution pins.
			return false;
		};

		FUeremcpEdGraphReadOptions Options;
		Options.AssetPath = AssetPath;
		Options.GraphName = Entry.Name;
		Options.GraphType = GraphType;
		Options.bRoundTripSupported = false;
		Options.LossyAreas = {
			TEXT("anim_graph_authoring_unsupported"),
			TEXT("anim_node_internal_state_partial"),
		};
		if (GraphType == TEXT("AnimStateMachine"))
		{
			Options.LossyAreas.Add(TEXT("anim_state_machine_authoring_unsupported"));
		}
		Options.PurposeSummaryPrefix = TEXT("AnimBlueprint graph");

		FUeremcpEdGraphReadResult ReadResult;
		if (!FUeremcpEdGraphReader::ReadGraph(Graph, Options, Hooks, ReadResult))
		{
			OutError = FString::Printf(
				TEXT("Failed to read AnimBlueprint graph '%s': %s"),
				*Entry.Name,
				*ReadResult.Error);
			return false;
		}

		TSharedPtr<FJsonObject> Extensions = MakeShared<FJsonObject>();
		Extensions->SetObjectField(TEXT("animation"), AnimationExtension);
		ReadResult.Graph->SetObjectField(TEXT("extensions"), Extensions);

		TArray<FString> SubgraphIds;
		for (UEdGraph* Subgraph : Graph->SubGraphs)
		{
			if (Subgraph)
			{
				SubgraphIds.AddUnique(Subgraph->GetName());
			}
		}
		SubgraphIds.Sort();
		if (SubgraphIds.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> SubgraphsJson;
			for (const FString& SubgraphId : SubgraphIds)
			{
				SubgraphsJson.Add(MakeShared<FJsonValueString>(SubgraphId));
			}
			ReadResult.Graph->SetArrayField(TEXT("subgraphs"), SubgraphsJson);
		}

		for (const FString& DependencyPath : ReadResult.DependencyPaths)
		{
			OutInspection.DependencyPaths.AddUnique(DependencyPath);
		}
		GraphsJson.Add(JsonObjectValue(ReadResult.Graph));
		OutInspection.NodeCount += NodeCount;
		if (GraphType == TEXT("AnimBlueprintGraph"))
		{
			++OutInspection.AnimGraphCount;
		}
		else if (GraphType == TEXT("AnimStateMachine"))
		{
			++OutInspection.StateMachineCount;
		}
	}
	Inventory->SetArrayField(TEXT("graphs"), GraphsJson);
	OutInspection.GraphCount = GraphsJson.Num();

	OutInspection.ContentHash = FUeremcpContentHash::HashJsonObject(Inventory, &OutError);
	if (OutInspection.ContentHash.IsEmpty())
	{
		return false;
	}

	Inventory->SetStringField(TEXT("content_hash"), OutInspection.ContentHash);
	Inventory->SetStringField(TEXT("revision"), OutInspection.ContentHash);
	OutInspection.Inventory = MoveTemp(Inventory);
	return true;
}
