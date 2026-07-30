#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_ResetDynamics.h"
#include "Animation/AnimNotifies/AnimNotifyState_DisableRootMotion.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpAnimationService.h"
#include "UeremcpAnimationToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

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
	StateEvent.SetDuration(0.3f);
	StateEvent.TrackIndex = 0;
	StateEvent.NotifyStateClass = NewObject<UAnimNotifyState_DisableRootMotion>(Montage);
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
				TestTrue(TEXT("notify time"), FMath::IsNearlyEqual(NotifyObject->GetNumberField(TEXT("time")), 0.5));
				TestTrue(TEXT("notify duration"), FMath::IsNearlyEqual(NotifyObject->GetNumberField(TEXT("duration")), 0.2));
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
	FUeremcpAnimationInspectMontageEnvelopeTest,
	"UEREMCP.Animation.InspectMontage.EnvelopeRejections",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpAnimationInspectMontageEnvelopeTest::RunTest(const FString& Parameters)
{
	const FString Malformed = UUeremcpAnimationToolset::InspectMontage(TEXT("not-json"));
	TestTrue(TEXT("malformed request rejected"), Malformed.Contains(TEXT("\"status\":\"rejected\"")));

	const FString WrongAction = UUeremcpAnimationToolset::InspectMontage(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-wrong","action":"inspect_sequence","target":{"asset_path":"/Game/None"}})"));
	TestTrue(TEXT("wrong action rejected"), WrongAction.Contains(TEXT("\"status\":\"rejected\"")));

	const FString MissingTarget = UUeremcpAnimationToolset::InspectMontage(
		TEXT(R"({"protocol_version":"1.0","request_id":"ws10-missing","action":"inspect_montage"})"));
	TestTrue(TEXT("missing target rejected"), MissingTarget.Contains(TEXT("\"status\":\"rejected\"")));
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
