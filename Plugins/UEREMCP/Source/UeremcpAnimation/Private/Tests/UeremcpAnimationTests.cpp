#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_ResetDynamics.h"
#include "Animation/AnimNotifies/AnimNotifyState_DisableRootMotion.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpAnimationService.h"
#include "UeremcpAnimationToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ParseResponseObject(const FString& ResponseJson)
	{
		TSharedPtr<FJsonObject> Response;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
		if (!FJsonSerializer::Deserialize(Reader, Response) || !Response.IsValid())
		{
			return nullptr;
		}
		return Response;
	}

	FString ResponseStatus(const FString& ResponseJson)
	{
		const TSharedPtr<FJsonObject> Response = ParseResponseObject(ResponseJson);
		FString Status;
		if (Response.IsValid())
		{
			Response->TryGetStringField(TEXT("status"), Status);
		}
		return Status;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationInspectMontageServiceTest,
	"UEREMCP.Animation.InspectMontage.StructuredState",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationInspectMontageServiceTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("AM_WS10_Inspect"));
	Montage->SlotAnimTracks.Reset();
	FSlotAnimationTrack& Slot = Montage->AddSlot(TEXT("UpperBody"));

	UAnimSequence* Sequence = NewObject<UAnimSequence>(GetTransientPackage(), TEXT("A_WS10_Source"));
	// A transient UAnimSequence has no MovieScene until its sequencer data model is
	// initialized; FAnimSegment::SetAnimReference immediately queries that model.
	// [VERIFIED: AnimSequencerController.cpp:2509-2524; AnimCompositeBase.cpp:753-759,785-792]
	Sequence->GetController().InitializeModel();
	FAnimSegment Segment;
	Segment.SetAnimReference(Sequence);
	Segment.StartPos = 0.25f;
	Segment.AnimStartTime = 0.1f;
	Segment.AnimEndTime = 0.9f;
	Segment.AnimPlayRate = 1.5f;
	Segment.LoopingCount = 2;
	Slot.AnimTrack.AnimSegments.Add(Segment);

	FCompositeSection Section;
	Section.SectionName = TEXT("Attack");
	Section.SetTime(0.25f);
	Montage->CompositeSections.Add(Section);

#if WITH_EDITORONLY_DATA
	Montage->AnimNotifyTracks.Reset();
	Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("Gameplay"), FLinearColor::White));
#endif
	FAnimNotifyEvent Event;
	Event.NotifyName = TEXT("Impact");
	Event.SetTime(0.5f);
	Event.SetDuration(0.2f);
	Event.TrackIndex = 0;
	Event.NotifyTriggerChance = 0.75f;
	Event.bTriggerOnDedicatedServer = false;
	Event.Notify = NewObject<UAnimNotify_ResetDynamics>(Montage);
	Montage->Notifies.Add(Event);

	FAnimNotifyEvent StateEvent;
	StateEvent.NotifyName = TEXT("DisableRootMotion");
	StateEvent.SetTime(0.6f);
	StateEvent.TrackIndex = 0;
	StateEvent.NotifyStateClass = NewObject<UAnimNotifyState_DisableRootMotion>(Montage);
	// UE creates the state object before assigning duration; GetDuration only
	// returns EndLink - start time when NotifyStateClass is set.
	// [VERIFIED: AnimationBlueprintLibrary.cpp:796-826; AnimTypes.cpp:96-105]
	StateEvent.SetDuration(0.3f);
	Montage->Notifies.Add(StateEvent);

	FUeremcpMontageInspection Inspection;
	FString Error;
	TestTrue(
		TEXT("transient montage inspection succeeds"),
		FUeremcpAnimationService::InspectMontage(
			Montage,
			TEXT("/Game/__UeremcpTests/Animation/AM_WS10_Inspect"),
			Inspection,
			Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	TestTrue(TEXT("state returned"), Inspection.State.IsValid());
	TestTrue(TEXT("content hash returned"), Inspection.ContentHash.StartsWith(TEXT("sha256:")));
	TestEqual(TEXT("slot count"), Inspection.SlotCount, 1);
	TestEqual(TEXT("segment count"), Inspection.SegmentCount, 1);
	TestEqual(TEXT("section count"), Inspection.SectionCount, 1);
	TestEqual(TEXT("notify count"), Inspection.NotifyCount, 2);

	if (Inspection.State.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Notifies = nullptr;
		TestTrue(TEXT("slots array present"), Inspection.State->TryGetArrayField(TEXT("slots"), Slots));
		TestTrue(TEXT("notifies array present"), Inspection.State->TryGetArrayField(TEXT("notifies"), Notifies));
		TestEqual(TEXT("one serialized slot"), Slots ? Slots->Num() : 0, 1);
		TestEqual(TEXT("two serialized notifies"), Notifies ? Notifies->Num() : 0, 2);

		if (Notifies && Notifies->Num() == 2)
		{
			const TSharedPtr<FJsonObject> NotifyObject = (*Notifies)[0]->AsObject();
			const TSharedPtr<FJsonObject> StateObject = (*Notifies)[1]->AsObject();
			TestTrue(TEXT("notify object serialized"), NotifyObject.IsValid());
			TestTrue(TEXT("notify state object serialized"), StateObject.IsValid());
			if (NotifyObject.IsValid())
			{
				TestEqual(TEXT("notify name"), NotifyObject->GetStringField(TEXT("name")), FString(TEXT("Impact")));
				TestEqual(TEXT("notify track"), NotifyObject->GetStringField(TEXT("track")), FString(TEXT("Gameplay")));
				TestEqual(
					TEXT("notify track index"),
					static_cast<int32>(NotifyObject->GetNumberField(TEXT("track_index"))),
					0);
				TestTrue(TEXT("notify time"), FMath::IsNearlyEqual(NotifyObject->GetNumberField(TEXT("time")), 0.5));
				TestTrue(
					TEXT("instant notify duration is zero"),
					FMath::IsNearlyZero(NotifyObject->GetNumberField(TEXT("duration"))));
				TestTrue(TEXT("notify trigger chance"), FMath::IsNearlyEqual(NotifyObject->GetNumberField(TEXT("trigger_chance")), 0.75));
				TestFalse(TEXT("notify dedicated-server policy"), NotifyObject->GetBoolField(TEXT("trigger_on_dedicated_server")));
				TestFalse(TEXT("notify is not a state"), NotifyObject->GetBoolField(TEXT("is_state")));
				TestTrue(
					TEXT("notify concrete class"),
					NotifyObject->GetStringField(TEXT("class")).Contains(TEXT("AnimNotify_ResetDynamics")));
			}
			if (StateObject.IsValid())
			{
				TestEqual(TEXT("state name"), StateObject->GetStringField(TEXT("name")), FString(TEXT("DisableRootMotion")));
				TestTrue(
					TEXT("notify state duration"),
					FMath::IsNearlyEqual(StateObject->GetNumberField(TEXT("duration")), 0.3));
				TestTrue(TEXT("state is marked as state"), StateObject->GetBoolField(TEXT("is_state")));
				TestTrue(
					TEXT("state concrete class"),
					StateObject->GetStringField(TEXT("class")).Contains(TEXT("AnimNotifyState_DisableRootMotion")));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationInspectMontageRevisionTest,
	"UEREMCP.Animation.InspectMontage.RevisionStability",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationInspectMontageRevisionTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("AM_WS10_Revision"));
#if WITH_EDITORONLY_DATA
	Montage->AnimNotifyTracks.Reset();
	Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("Gameplay"), FLinearColor::White));
#endif

	FAnimNotifyEvent Event;
	Event.NotifyName = TEXT("Impact");
	Event.SetTime(0.5f);
	Event.TrackIndex = 0;
	Event.NotifyTriggerChance = 1.0f;
	Event.Notify = NewObject<UAnimNotify_ResetDynamics>(Montage);
	Montage->Notifies.Add(Event);

	auto Inspect = [this, Montage](FUeremcpMontageInspection& Out) -> bool
	{
		FString Error;
		const bool bOk = FUeremcpAnimationService::InspectMontage(
			Montage,
			TEXT("/Game/__UeremcpTests/Animation/AM_WS10_Revision"),
			Out,
			Error);
		if (!bOk)
		{
			AddError(Error);
		}
		return bOk;
	};

	FUeremcpMontageInspection First;
	FUeremcpMontageInspection Repeat;
	TestTrue(TEXT("first inspection succeeds"), Inspect(First));
	TestTrue(TEXT("repeat inspection succeeds"), Inspect(Repeat));
	TestEqual(TEXT("unchanged montage has stable revision"), Repeat.ContentHash, First.ContentHash);

#if WITH_EDITORONLY_DATA
	Montage->Notifies[0].Guid = FGuid::NewGuid();
	FUeremcpMontageInspection GuidChurn;
	TestTrue(TEXT("GUID-churn inspection succeeds"), Inspect(GuidChurn));
	TestEqual(TEXT("editor notify GUID does not change revision"), GuidChurn.ContentHash, First.ContentHash);
#endif

	Montage->Notifies[0].NotifyTriggerChance = 0.5f;
	FUeremcpMontageInspection SemanticChange;
	TestTrue(TEXT("semantic-change inspection succeeds"), Inspect(SemanticChange));
	TestNotEqual(
		TEXT("notify trigger policy changes revision"),
		SemanticChange.ContentHash,
		First.ContentHash);

	Montage->Notifies[0].NotifyTriggerChance = 1.0f;
	FUeremcpMontageInspection Restored;
	TestTrue(TEXT("restored inspection succeeds"), Inspect(Restored));
	TestEqual(TEXT("restoring semantic state restores revision"), Restored.ContentHash, First.ContentHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationInspectMontageNotifyOrderingTest,
	"UEREMCP.Animation.InspectMontage.NotifyOrdering",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationInspectMontageNotifyOrderingTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = NewObject<UAnimMontage>(
		GetTransientPackage(),
		TEXT("AM_WS10_NotifyOrdering"));
#if WITH_EDITORONLY_DATA
	Montage->AnimNotifyTracks.Reset();
	Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("Gameplay"), FLinearColor::White));
#endif

	FAnimNotifyEvent LateEvent;
	LateEvent.NotifyName = TEXT("Late");
	LateEvent.SetTime(0.75f);
	LateEvent.TrackIndex = 0;
	LateEvent.Notify = NewObject<UAnimNotify_ResetDynamics>(Montage);
	Montage->Notifies.Add(LateEvent);

	FAnimNotifyEvent EarlyInvalidTrackEvent;
	EarlyInvalidTrackEvent.NotifyName = TEXT("EarlyInvalidTrack");
	EarlyInvalidTrackEvent.SetTime(0.25f);
	EarlyInvalidTrackEvent.TrackIndex = -1;
	Montage->Notifies.Add(EarlyInvalidTrackEvent);

	auto Inspect = [this, Montage](FUeremcpMontageInspection& Out) -> bool
	{
		FString Error;
		const bool bOk = FUeremcpAnimationService::InspectMontage(
			Montage,
			TEXT("/Game/__UeremcpTests/Animation/AM_WS10_NotifyOrdering"),
			Out,
			Error);
		if (!bOk)
		{
			AddError(Error);
		}
		return bOk;
	};

	FUeremcpMontageInspection First;
	TestTrue(TEXT("notify ordering inspection succeeds"), Inspect(First));
	const TArray<TSharedPtr<FJsonValue>>* FirstNotifies = nullptr;
	TestTrue(
		TEXT("ordered notify array returned"),
		First.State.IsValid()
			&& First.State->TryGetArrayField(TEXT("notifies"), FirstNotifies)
			&& FirstNotifies);
	TestEqual(TEXT("two notify edge cases returned"), FirstNotifies ? FirstNotifies->Num() : 0, 2);
	if (FirstNotifies && FirstNotifies->Num() == 2)
	{
		const TSharedPtr<FJsonObject> Early = (*FirstNotifies)[0]->AsObject();
		const TSharedPtr<FJsonObject> Late = (*FirstNotifies)[1]->AsObject();
		TestEqual(
			TEXT("earlier trigger is serialized first"),
			Early->GetStringField(TEXT("name")),
			FString(TEXT("EarlyInvalidTrack")));
		TestEqual(
			TEXT("invalid track index is retained"),
			static_cast<int32>(Early->GetNumberField(TEXT("track_index"))),
			-1);
		TestTrue(TEXT("invalid track name degrades to empty"), Early->GetStringField(TEXT("track")).IsEmpty());
		const TSharedPtr<FJsonValue> ClassValue = Early->TryGetField(TEXT("class"));
		TestTrue(
			TEXT("notify without object has null class"),
			ClassValue.IsValid() && ClassValue->Type == EJson::Null);
		TestEqual(
			TEXT("later trigger is serialized second"),
			Late->GetStringField(TEXT("name")),
			FString(TEXT("Late")));
	}

	Montage->Notifies.Swap(0, 1);
	FUeremcpMontageInspection ReorderedStorage;
	TestTrue(TEXT("reordered storage inspection succeeds"), Inspect(ReorderedStorage));
	TestEqual(
		TEXT("raw notify storage order does not change revision"),
		ReorderedStorage.ContentHash,
		First.ContentHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationInspectMontageEditorScratchTest,
	"UEREMCP.Animation.InspectMontage.EditorScratchAsset",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationInspectMontageEditorScratchTest::RunTest(const FString& Parameters)
{
	const FString AssetName = FString::Printf(
		TEXT("AM_WS10_Editor_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString PackagePath = FString::Printf(
		TEXT("/Game/__UeremcpTests/Animation/%s"),
		*AssetName);

	UPackage* Package = CreatePackage(*PackagePath);
	TestNotNull(TEXT("scratch package created"), Package);
	if (!Package)
	{
		return false;
	}

	UAnimMontage* Montage = NewObject<UAnimMontage>(
		Package,
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);
	TestNotNull(TEXT("scratch montage created"), Montage);
	if (!Montage)
	{
		Package->MarkAsGarbage();
		return false;
	}

	Montage->SlotAnimTracks.Reset();
	Montage->AddSlot(TEXT("UpperBody"));
#if WITH_EDITORONLY_DATA
	Montage->AnimNotifyTracks.Reset();
	Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("Gameplay"), FLinearColor::White));
#endif

	FAnimNotifyEvent Event;
	Event.NotifyName = TEXT("Impact");
	Event.SetTime(0.25f);
	Event.TrackIndex = 0;
	Event.Notify = NewObject<UAnimNotify_ResetDynamics>(Montage);
	Montage->Notifies.Add(Event);

	const FString Request = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-editor-scratch","action":"inspect_montage","target":{"asset_path":"%s"},"options":{"response_detail":"complete"}})"),
		*PackagePath);
	const FString ResponseJson = UUeremcpAnimationToolset::InspectMontage(Request);

	TSharedPtr<FJsonObject> Response;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
	TestTrue(
		TEXT("tool response is parseable JSON"),
		FJsonSerializer::Deserialize(Reader, Response) && Response.IsValid());
	FString PackagePathRevision;
	if (Response.IsValid())
	{
		TestEqual(
			TEXT("response remains honest before asset_state amendment"),
			Response->GetStringField(TEXT("status")),
			FString(TEXT("partially_completed")));
		TestTrue(
			TEXT("summary reports real notify enumeration"),
			Response->GetStringField(TEXT("summary")).Contains(TEXT("1 real notify events")));
		TestTrue(
			TEXT("revision returned"),
			Response->GetStringField(TEXT("revision")).StartsWith(TEXT("sha256:")));
		PackagePathRevision = Response->GetStringField(TEXT("revision"));

		const TSharedPtr<FJsonObject>* Result = nullptr;
		TestTrue(
			TEXT("result returned"),
			Response->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid());
		if (Result && Result->IsValid())
		{
			TestEqual(
				TEXT("primary asset keeps package path contract"),
				(*Result)->GetStringField(TEXT("primary_asset")),
				PackagePath);
		}

		const TSharedPtr<FJsonObject>* Validation = nullptr;
		TestTrue(
			TEXT("validation returned"),
			Response->TryGetObjectField(TEXT("validation"), Validation)
				&& Validation
				&& Validation->IsValid());
		if (Validation && Validation->IsValid())
		{
			TestTrue(
				TEXT("scratch montage structurally inspected"),
				(*Validation)->GetBoolField(TEXT("structurally_valid")));
		}
	}

	const FString ObjectPath = FString::Printf(
		TEXT("%s.%s"),
		*PackagePath,
		*AssetName);
	const FString ObjectPathRequest = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-editor-object-path","action":"inspect_montage","target":{"asset_path":"%s"},"options":{"response_detail":"complete"}})"),
		*ObjectPath);
	const FString ObjectPathResponseJson =
		UUeremcpAnimationToolset::InspectMontage(ObjectPathRequest);
	TSharedPtr<FJsonObject> ObjectPathResponse;
	const TSharedRef<TJsonReader<>> ObjectPathReader =
		TJsonReaderFactory<>::Create(ObjectPathResponseJson);
	TestTrue(
		TEXT("full object path response is parseable"),
		FJsonSerializer::Deserialize(ObjectPathReader, ObjectPathResponse)
			&& ObjectPathResponse.IsValid());
	if (ObjectPathResponse.IsValid())
	{
		TestEqual(
			TEXT("full object path remains honest partial"),
			ObjectPathResponse->GetStringField(TEXT("status")),
			FString(TEXT("partially_completed")));
		TestEqual(
			TEXT("package and object paths produce one canonical revision"),
			ObjectPathResponse->GetStringField(TEXT("revision")),
			PackagePathRevision);

		const TSharedPtr<FJsonObject>* ObjectPathResult = nullptr;
		TestTrue(
			TEXT("full object path result returned"),
			ObjectPathResponse->TryGetObjectField(TEXT("result"), ObjectPathResult)
				&& ObjectPathResult
				&& ObjectPathResult->IsValid());
		if (ObjectPathResult && ObjectPathResult->IsValid())
		{
			TestEqual(
				TEXT("full object path canonicalizes to package primary asset"),
				(*ObjectPathResult)->GetStringField(TEXT("primary_asset")),
				PackagePath);
		}
	}

	// The fixture is unique, in-memory only, and never saved to user content.
	Montage->ClearFlags(RF_Public | RF_Standalone);
	Montage->MarkAsGarbage();
	Package->SetDirtyFlag(false);
	Package->MarkAsGarbage();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationInspectMontageEnvelopeTest,
	"UEREMCP.Animation.InspectMontage.EnvelopeRejections",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationInspectMontageEnvelopeTest::RunTest(const FString& Parameters)
{
	const FString Malformed = UUeremcpAnimationToolset::InspectMontage(TEXT("not-json"));
	TestEqual(TEXT("malformed request rejected"), ResponseStatus(Malformed), FString(TEXT("rejected")));

	const FString WrongAction = UUeremcpAnimationToolset::InspectMontage(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-wrong","action":"inspect_sequence","target":{"asset_path":"/Game/None"}})"));
	TestEqual(TEXT("wrong action rejected"), ResponseStatus(WrongAction), FString(TEXT("rejected")));

	const FString MissingTarget = UUeremcpAnimationToolset::InspectMontage(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-missing","action":"inspect_montage"})"));
	TestEqual(TEXT("missing target rejected"), ResponseStatus(MissingTarget), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationReadAnimBpServiceTest,
	"UEREMCP.Animation.ReadAnimBp.GraphInventory",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationReadAnimBpServiceTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* AnimBP = NewObject<UAnimBlueprint>(GetTransientPackage(), TEXT("ABP_WS10_Inspect"));
	AnimBP->bIsTemplate = true;

	UAnimationGraph* AnimGraph = NewObject<UAnimationGraph>(AnimBP, TEXT("AnimGraph"));
	AnimGraph->GraphGuid = FGuid(1, 2, 3, 4);
	AnimBP->FunctionGraphs.Add(AnimGraph);
	AnimGraph->AddNode(NewObject<UEdGraphNode>(AnimGraph), false, false);

	UAnimationStateMachineGraph* StateMachine = NewObject<UAnimationStateMachineGraph>(
		AnimGraph, TEXT("Locomotion"));
	StateMachine->GraphGuid = FGuid(5, 6, 7, 8);
	AnimGraph->SubGraphs.Add(StateMachine);

	UEdGraph* EventGraph = NewObject<UEdGraph>(AnimBP, TEXT("EventGraph"));
	EventGraph->GraphGuid = FGuid(9, 10, 11, 12);
	AnimBP->UbergraphPages.Add(EventGraph);

	FUeremcpAnimBlueprintInspection Inspection;
	FString Error;
	TestTrue(
		TEXT("transient AnimBP inventory succeeds"),
		FUeremcpAnimationService::InspectAnimBlueprint(
			AnimBP,
			TEXT("/Game/__UeremcpTests/Animation/ABP_WS10_Inspect"),
			Inspection,
			Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	TestTrue(TEXT("inventory returned"), Inspection.Inventory.IsValid());
	TestTrue(TEXT("content hash returned"), Inspection.ContentHash.StartsWith(TEXT("sha256:")));
	TestEqual(TEXT("graph count"), Inspection.GraphCount, 3);
	TestEqual(TEXT("anim graph count"), Inspection.AnimGraphCount, 2);
	TestEqual(TEXT("state machine count"), Inspection.StateMachineCount, 1);
	TestEqual(TEXT("node count includes nested graph inventory"), Inspection.NodeCount, 1);

	if (Inspection.Inventory.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
		TestTrue(TEXT("graphs array present"), Inspection.Inventory->TryGetArrayField(TEXT("graphs"), Graphs));
		TestEqual(TEXT("three serialized graphs"), Graphs ? Graphs->Num() : 0, 3);

		TArray<FString> GraphNames;
		TSet<FString> GraphTypes;
		int32 FidelityCount = 0;
		int32 NodesAndLinksCount = 0;
		int32 AnimationExtensionCount = 0;
		if (Graphs)
		{
			for (const TSharedPtr<FJsonValue>& GraphValue : *Graphs)
			{
				const TSharedPtr<FJsonObject> GraphObject = GraphValue->AsObject();
				if (!GraphObject.IsValid())
				{
					continue;
				}
				GraphNames.Add(GraphObject->GetStringField(TEXT("graph_name")));
				GraphTypes.Add(GraphObject->GetStringField(TEXT("graph_type")));
				const TSharedPtr<FJsonObject>* Fidelity = nullptr;
				if (GraphObject->TryGetObjectField(TEXT("fidelity"), Fidelity) && Fidelity && (*Fidelity).IsValid())
				{
					++FidelityCount;
					TestFalse(
						TEXT("round-trip unsupported"),
						(*Fidelity)->GetBoolField(TEXT("round_trip_supported")));
				}
				const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
				if (GraphObject->TryGetArrayField(TEXT("nodes"), Nodes)
					&& GraphObject->TryGetArrayField(TEXT("links"), Links))
				{
					++NodesAndLinksCount;
				}
				const TSharedPtr<FJsonObject>* Extensions = nullptr;
				const TSharedPtr<FJsonObject>* Animation = nullptr;
				if (GraphObject->TryGetObjectField(TEXT("extensions"), Extensions)
					&& Extensions
					&& (*Extensions)->TryGetObjectField(TEXT("animation"), Animation)
					&& Animation
					&& (*Animation).IsValid())
				{
					++AnimationExtensionCount;
					TestTrue(
						TEXT("animation extension carries state machines"),
						(*Animation)->HasTypedField<EJson::Array>(TEXT("state_machines")));
				}
			}
		}
		TestEqual(TEXT("three graph names collected"), GraphNames.Num(), 3);
		if (GraphNames.Num() == 3)
		{
			TestEqual(TEXT("AnimGraph sorts first"), GraphNames[0], FString(TEXT("AnimGraph")));
			TestEqual(TEXT("EventGraph sorts second"), GraphNames[1], FString(TEXT("EventGraph")));
			TestEqual(TEXT("Locomotion sorts third"), GraphNames[2], FString(TEXT("Locomotion")));
		}
		TestTrue(TEXT("AnimGraph classified"), GraphTypes.Contains(TEXT("AnimBlueprintGraph")));
		TestTrue(TEXT("nested state machine classified"), GraphTypes.Contains(TEXT("AnimStateMachine")));
		TestEqual(TEXT("every graph carries round-trip fidelity"), FidelityCount, 3);
		TestEqual(TEXT("every graph emits nodes and links"), NodesAndLinksCount, 3);
		TestEqual(TEXT("every graph carries extensions.animation"), AnimationExtensionCount, 3);
	}

	FUeremcpAnimBlueprintInspection Reread;
	FString RereadError;
	TestTrue(
		TEXT("reread succeeds"),
		FUeremcpAnimationService::InspectAnimBlueprint(
			AnimBP,
			TEXT("/Game/__UeremcpTests/Animation/ABP_WS10_Inspect"),
			Reread,
			RereadError));
	TestEqual(TEXT("stable AnimBP revision"), Inspection.ContentHash, Reread.ContentHash);

	AnimGraph->GraphGuid = FGuid::NewGuid();
	StateMachine->GraphGuid = FGuid::NewGuid();
	FUeremcpAnimBlueprintInspection GuidChurn;
	FString GuidChurnError;
	TestTrue(
		TEXT("inspection after engine GUID churn succeeds"),
		FUeremcpAnimationService::InspectAnimBlueprint(
			AnimBP,
			TEXT("/Game/__UeremcpTests/Animation/ABP_WS10_Inspect"),
			GuidChurn,
			GuidChurnError));
	TestEqual(
		TEXT("engine graph GUID churn does not change semantic revision"),
		Inspection.ContentHash,
		GuidChurn.ContentHash);

	AnimGraph->AddNode(NewObject<UEdGraphNode>(AnimGraph), false, false);
	FUeremcpAnimBlueprintInspection NodeChange;
	FString NodeChangeError;
	TestTrue(
		TEXT("inspection after node-count change succeeds"),
		FUeremcpAnimationService::InspectAnimBlueprint(
			AnimBP,
			TEXT("/Game/__UeremcpTests/Animation/ABP_WS10_Inspect"),
			NodeChange,
			NodeChangeError));
	TestNotEqual(
		TEXT("node-count semantic change updates revision"),
		Inspection.ContentHash,
		NodeChange.ContentHash);
	TestEqual(TEXT("node-count semantic change is counted"), NodeChange.NodeCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationCrossAssetInspectionTest,
	"UEREMCP.Animation.CrossAsset.MontageAndAnimBpIsolation",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationCrossAssetInspectionTest::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = NewObject<USkeleton>(GetTransientPackage(), TEXT("SK_WS10_Shared"));

	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("AM_WS10_Cross"));
	Montage->SetSkeleton(Skeleton);
	Montage->SlotAnimTracks.Reset();
	Montage->AddSlot(TEXT("DefaultSlot"));

	UAnimBlueprint* AnimBP = NewObject<UAnimBlueprint>(GetTransientPackage(), TEXT("ABP_WS10_Cross"));
	AnimBP->TargetSkeleton = Skeleton;
	UAnimationGraph* AnimGraph = NewObject<UAnimationGraph>(AnimBP, TEXT("AnimGraph"));
	AnimBP->FunctionGraphs.Add(AnimGraph);

	FUeremcpMontageInspection MontageInspection;
	FString MontageError;
	TestTrue(
		TEXT("montage half of cross-fixture succeeds"),
		FUeremcpAnimationService::InspectMontage(
			Montage,
			TEXT("/Game/__UeremcpTests/Animation/AM_WS10_Cross"),
			MontageInspection,
			MontageError));

	FUeremcpAnimBlueprintInspection AnimBpInspection;
	FString AnimBpError;
	TestTrue(
		TEXT("AnimBP half of cross-fixture succeeds"),
		FUeremcpAnimationService::InspectAnimBlueprint(
			AnimBP,
			TEXT("/Game/__UeremcpTests/Animation/ABP_WS10_Cross"),
			AnimBpInspection,
			AnimBpError));

	TestEqual(TEXT("montage resolves shared skeleton once"), MontageInspection.DependencyPaths.Num(), 1);
	TestEqual(TEXT("AnimBP resolves shared skeleton once"), AnimBpInspection.DependencyPaths.Num(), 1);
	if (MontageInspection.DependencyPaths.Num() == 1 && AnimBpInspection.DependencyPaths.Num() == 1)
	{
		TestEqual(
			TEXT("cross-fixtures report the same skeleton dependency"),
			MontageInspection.DependencyPaths[0],
			AnimBpInspection.DependencyPaths[0]);
	}
	TestNotEqual(
		TEXT("different asset shapes retain independent revisions"),
		MontageInspection.ContentHash,
		AnimBpInspection.ContentHash);

	if (MontageInspection.State.IsValid() && AnimBpInspection.Inventory.IsValid())
	{
		TestTrue(TEXT("montage state retains slots"), MontageInspection.State->HasField(TEXT("slots")));
		TestFalse(TEXT("montage state does not leak graphs"), MontageInspection.State->HasField(TEXT("graphs")));
		TestTrue(TEXT("AnimBP inventory retains graphs"), AnimBpInspection.Inventory->HasField(TEXT("graphs")));
		TestFalse(TEXT("AnimBP inventory does not leak slots"), AnimBpInspection.Inventory->HasField(TEXT("slots")));
	}

	FUeremcpAnimBlueprintInspection NullInspection;
	NullInspection.GraphCount = 99;
	NullInspection.ContentHash = TEXT("stale");
	NullInspection.DependencyPaths.Add(TEXT("/Game/Stale"));
	FString NullError;
	TestFalse(
		TEXT("null AnimBP is rejected"),
		FUeremcpAnimationService::InspectAnimBlueprint(
			nullptr,
			TEXT("/Game/None"),
			NullInspection,
			NullError));
	TestEqual(TEXT("null rejection resets graph count"), NullInspection.GraphCount, 0);
	TestTrue(TEXT("null rejection clears stale revision"), NullInspection.ContentHash.IsEmpty());
	TestTrue(TEXT("null rejection clears stale dependencies"), NullInspection.DependencyPaths.IsEmpty());
	TestEqual(TEXT("null rejection explains failure"), NullError, FString(TEXT("AnimBlueprint is null.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationReadAnimBpEditorScratchTest,
	"UEREMCP.Animation.ReadAnimBp.EditorScratchAsset",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationReadAnimBpEditorScratchTest::RunTest(const FString& Parameters)
{
	const FString PackageName = FString::Printf(
		TEXT("/Game/__UeremcpTests/Animation/ABP_WS10_Scratch_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UPackage* Package = CreatePackage(*PackageName);
	Package->SetFlags(RF_Transient);
	Package->SetDirtyFlag(false);

	UAnimBlueprint* AnimBP = NewObject<UAnimBlueprint>(
		Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Transient);
	AnimBP->bIsTemplate = true;
	UAnimationGraph* AnimGraph = NewObject<UAnimationGraph>(AnimBP, TEXT("AnimGraph"));
	AnimBP->FunctionGraphs.Add(AnimGraph);

	const FString Request = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-abp-scratch","action":"read_anim_bp","target":{"asset_path":"%s"}})"),
		*PackageName);
	const FString ResponseJson = UUeremcpAnimationToolset::ReadAnimBp(Request);

	TestEqual(
		TEXT("scratch AnimBP inspect is partial"),
		ResponseStatus(ResponseJson),
		FString(TEXT("partially_completed")));
	TestTrue(TEXT("summary names structured read"), ResponseJson.Contains(TEXT("Read AnimBlueprint")));
	TestFalse(TEXT("response remains honest before asset_state amendment"), ResponseJson.Contains(TEXT("\"asset_state\"")));
	TestTrue(TEXT("nodes-and-links check recorded"), ResponseJson.Contains(TEXT("animation.anim_bp.nodes_and_links_read")));

	const FString ObjectPath = FString::Printf(
		TEXT("%s.%s"), *PackageName, *FPackageName::GetLongPackageAssetName(PackageName));
	const FString ObjectPathRequest = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-abp-object","action":"read_anim_bp","target":{"asset_path":"%s"}})"),
		*ObjectPath);
	const FString ObjectPathResponse = UUeremcpAnimationToolset::ReadAnimBp(ObjectPathRequest);
	const TSharedPtr<FJsonObject> ObjectPathRoot = ParseResponseObject(ObjectPathResponse);
	const TSharedPtr<FJsonObject>* ObjectPathResult = nullptr;
	TestTrue(TEXT("object-path response parses"), ObjectPathRoot.IsValid());
	TestTrue(
		TEXT("object-path response carries result"),
		ObjectPathRoot.IsValid()
			&& ObjectPathRoot->TryGetObjectField(TEXT("result"), ObjectPathResult)
			&& ObjectPathResult
			&& (*ObjectPathResult).IsValid());
	if (ObjectPathResult && (*ObjectPathResult).IsValid())
	{
		TestEqual(
			TEXT("package and object paths produce one canonical primary"),
			(*ObjectPathResult)->GetStringField(TEXT("primary_asset")),
			PackageName);
	}

	AnimBP->ClearFlags(RF_Public | RF_Standalone);
	AnimBP->MarkAsGarbage();
	Package->SetDirtyFlag(false);
	Package->MarkAsGarbage();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationReadAnimBpEnvelopeTest,
	"UEREMCP.Animation.ReadAnimBp.EnvelopeRejections",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationReadAnimBpEnvelopeTest::RunTest(const FString& Parameters)
{
	const FString Malformed = UUeremcpAnimationToolset::ReadAnimBp(TEXT("not-json"));
	TestEqual(TEXT("malformed request rejected"), ResponseStatus(Malformed), FString(TEXT("rejected")));

	const FString WrongAction = UUeremcpAnimationToolset::ReadAnimBp(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-abp-wrong","action":"inspect_montage","target":{"asset_path":"/Game/None"}})"));
	TestEqual(TEXT("wrong action rejected"), ResponseStatus(WrongAction), FString(TEXT("rejected")));

	const FString MissingTarget = UUeremcpAnimationToolset::ReadAnimBp(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-abp-missing","action":"read_anim_bp"})"));
	TestEqual(TEXT("missing target rejected"), ResponseStatus(MissingTarget), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationToolsetRegistrationTest,
	"UEREMCP.Animation.Toolset.Registration",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationToolsetRegistrationTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpAnimationToolset::StaticClass()))
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpAnimationToolset::StaticClass());
	}
	TestTrue(
		TEXT("animation toolset registered"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpAnimationToolset::StaticClass()));
	TestFalse(
		TEXT("animation toolset schema generated"),
		UToolsetRegistry::GetToolsetJsonSchema(UUeremcpAnimationToolset::StaticClass()).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
