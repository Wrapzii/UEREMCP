#include "UeremcpAnimationService.h"

#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "UeremcpContentHash.h"

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
		if (Cast<UAnimationGraph>(Graph))
		{
			return TEXT("AnimBlueprintGraph");
		}
		if (Graph && Graph->GetFName() == TEXT("EventGraph"))
		{
			return TEXT("EventGraph");
		}
		return TEXT("EdGraph");
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

	TArray<UAnimationGraph*> AnimationGraphs;
	UAnimationBlueprintLibrary::GetAnimationGraphs(AnimBlueprint, AnimationGraphs);
	TSet<const UEdGraph*> AnimationGraphSet;
	for (const UAnimationGraph* AnimationGraph : AnimationGraphs)
	{
		AnimationGraphSet.Add(AnimationGraph);
	}

	TArray<UEdGraph*> AllGraphs;
	AnimBlueprint->GetAllGraphs(AllGraphs);

	struct FGraphSortKey
	{
		FString Name;
		FString Guid;
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
		Key.Guid = Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
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
		return A.Guid < B.Guid;
	});

	TArray<TSharedPtr<FJsonValue>> GraphsJson;
	for (const FGraphSortKey& Entry : Sorted)
	{
		UEdGraph* Graph = Entry.Graph;
		const FString GraphType = ClassifyAnimBlueprintGraph(Graph);
		const bool bIsAnimationGraph = AnimationGraphSet.Contains(Graph);
		const int32 NodeCount = Graph->Nodes.Num();

		TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
		GraphObject->SetStringField(TEXT("name"), Entry.Name);
		GraphObject->SetStringField(TEXT("graph_guid"), Entry.Guid);
		GraphObject->SetStringField(TEXT("graph_class"), Graph->GetClass()->GetPathName());
		GraphObject->SetStringField(TEXT("graph_type"), GraphType);
		GraphObject->SetNumberField(TEXT("node_count"), NodeCount);
		GraphObject->SetBoolField(TEXT("is_animation_graph"), bIsAnimationGraph);

		TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
		Fidelity->SetBoolField(TEXT("inventory_complete"), true);
		Fidelity->SetBoolField(TEXT("nodes_emitted"), false);
		Fidelity->SetBoolField(TEXT("links_emitted"), false);
		Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
		GraphObject->SetObjectField(TEXT("fidelity"), Fidelity);

		GraphsJson.Add(JsonObjectValue(GraphObject));
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
