#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimMontage.h"
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
	Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("Gameplay"), FLinearColor::White));
#endif
	FAnimNotifyEvent Event;
	Event.NotifyName = TEXT("Impact");
	Event.SetTime(0.5f);
	Event.SetDuration(0.2f);
	Event.TrackIndex = 0;
	Montage->Notifies.Add(Event);

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
	TestEqual(TEXT("notify count"), Inspection.NotifyCount, 1);

	if (Inspection.State.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Notifies = nullptr;
		TestTrue(TEXT("slots array present"), Inspection.State->TryGetArrayField(TEXT("slots"), Slots));
		TestTrue(TEXT("notifies array present"), Inspection.State->TryGetArrayField(TEXT("notifies"), Notifies));
		TestEqual(TEXT("one serialized slot"), Slots ? Slots->Num() : 0, 1);
		TestEqual(TEXT("one serialized notify"), Notifies ? Notifies->Num() : 0, 1);
	}

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
