// Editor automation tests for animation.submit_montage_sections (WS-10).
//
// The contracts these tests defend:
//   1. dry_run is the default and plans without mutating.
//   2. Submission is COMPLETE-STATE — an unlisted section is removed, not left behind.
//   3. modified_and_validated is only returned after a post-write re-read agrees.
//   4. Invalid section sets are rejected before anything is touched.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Animation/AnimMontage.h"
#include "UeremcpAnimationToolset.h"
#include "UeremcpMontageSectionWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpMontageSectionTest
{
	/**
	 * A transient montage is enough for section work: composite sections are a plain
	 * array on the montage and do not require a skeleton or slot segments.
	 */
	static UAnimMontage* MakeScratchMontage()
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(
			GetTransientPackage(),
			UAnimMontage::StaticClass(),
			NAME_None,
			RF_Transient);
		return Montage;
	}

	static void SeedSection(UAnimMontage* Montage, const TCHAR* Name, float StartTime)
	{
		Montage->AddAnimCompositeSection(FName(Name), StartTime);
	}

	static TSharedPtr<FJsonObject> Parse(const FString& Json, FAutomationTestBase& Test)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!Test.TestTrue(TEXT("response parses as JSON"), FJsonSerializer::Deserialize(Reader, Object)))
		{
			return nullptr;
		}
		return Object;
	}

	static TSharedPtr<FJsonObject> GetSectionsDiagnostic(
		const TSharedPtr<FJsonObject>& Response,
		FAutomationTestBase& Test)
	{
		if (!Response.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		if (!Test.TestTrue(
				TEXT("response has diagnostics"),
				Response->TryGetObjectField(TEXT("diagnostics"), Diagnostics)))
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Sections = nullptr;
		if (!Test.TestTrue(
				TEXT("diagnostics has montage_sections"),
				(*Diagnostics)->TryGetObjectField(TEXT("montage_sections"), Sections)))
		{
			return nullptr;
		}
		return *Sections;
	}

	static int32 CountPlanEntries(
		const TSharedPtr<FJsonObject>& Sections,
		const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Plan = nullptr;
		if (!Sections.IsValid() || !Sections->TryGetObjectField(TEXT("plan"), Plan))
		{
			return -1;
		}
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!(*Plan)->TryGetArrayField(Field, Entries))
		{
			return -1;
		}
		return Entries->Num();
	}
}

// The service layer is testable without the envelope, so the plan/validate contracts
// are pinned directly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMontageSectionPlanTest,
	"UEREMCP.Animation.MontageSections.Plan",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMontageSectionPlanTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMontageSectionTest;

	UAnimMontage* Montage = MakeScratchMontage();
	if (!TestNotNull(TEXT("scratch montage created"), Montage))
	{
		return false;
	}
	SeedSection(Montage, TEXT("Old"), 0.f);
	SeedSection(Montage, TEXT("Keep"), 1.f);

	TArray<FUeremcpMontageSectionSpec> Specs;
	{
		FUeremcpMontageSectionSpec Keep;
		Keep.Name = TEXT("Keep");
		Keep.StartTime = 1.f;
		Specs.Add(Keep);

		FUeremcpMontageSectionSpec New;
		New.Name = TEXT("New");
		New.StartTime = 2.f;
		Specs.Add(New);
	}

	FUeremcpMontageSectionPlan Plan;
	FUeremcpMontageSectionWriter::BuildPlan(Montage, Specs, Plan);

	TestEqual(TEXT("one section to add"), Plan.ToAdd.Num(), 1);
	TestEqual(TEXT("added section is New"), Plan.ToAdd[0], FString(TEXT("New")));
	TestEqual(TEXT("one section to remove"), Plan.ToRemove.Num(), 1);
	TestEqual(TEXT("removed section is Old"), Plan.ToRemove[0], FString(TEXT("Old")));
	TestEqual(TEXT("one section unchanged"), Plan.Unchanged.Num(), 1);
	TestEqual(TEXT("unchanged section is Keep"), Plan.Unchanged[0], FString(TEXT("Keep")));
	TestTrue(TEXT("plan reports changes"), Plan.HasChanges());

	// BuildPlan must not have mutated anything.
	TestEqual(TEXT("BuildPlan is non-mutating"), Montage->CompositeSections.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMontageSectionValidationTest,
	"UEREMCP.Animation.MontageSections.Validation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMontageSectionValidationTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMontageSectionTest;

	UAnimMontage* Montage = MakeScratchMontage();
	if (!TestNotNull(TEXT("scratch montage created"), Montage))
	{
		return false;
	}

	FString Error;

	// Empty set would leave an unplayable montage.
	TestFalse(
		TEXT("empty section set is rejected"),
		FUeremcpMontageSectionWriter::ValidateSpecs(Montage, {}, Error));

	// Duplicate names would make AddAnimCompositeSection return INDEX_NONE.
	{
		TArray<FUeremcpMontageSectionSpec> Specs;
		FUeremcpMontageSectionSpec A;
		A.Name = TEXT("Same");
		Specs.Add(A);
		Specs.Add(A);
		TestFalse(
			TEXT("duplicate section names are rejected"),
			FUeremcpMontageSectionWriter::ValidateSpecs(Montage, Specs, Error));
	}

	// A chain target outside the submitted set would dangle after replacement.
	{
		TArray<FUeremcpMontageSectionSpec> Specs;
		FUeremcpMontageSectionSpec A;
		A.Name = TEXT("Start");
		A.NextSection = TEXT("Missing");
		A.bHasNextSection = true;
		Specs.Add(A);
		TestFalse(
			TEXT("next_section outside the submitted set is rejected"),
			FUeremcpMontageSectionWriter::ValidateSpecs(Montage, Specs, Error));
	}

	// Negative start time.
	{
		TArray<FUeremcpMontageSectionSpec> Specs;
		FUeremcpMontageSectionSpec A;
		A.Name = TEXT("Neg");
		A.StartTime = -1.f;
		Specs.Add(A);
		TestFalse(
			TEXT("negative start_time is rejected"),
			FUeremcpMontageSectionWriter::ValidateSpecs(Montage, Specs, Error));
	}

	// A self-chaining loop section is legal.
	{
		TArray<FUeremcpMontageSectionSpec> Specs;
		FUeremcpMontageSectionSpec A;
		A.Name = TEXT("Loop");
		A.NextSection = TEXT("Loop");
		A.bHasNextSection = true;
		Specs.Add(A);
		TestTrue(
			TEXT("self-chaining loop section is accepted"),
			FUeremcpMontageSectionWriter::ValidateSpecs(Montage, Specs, Error));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMontageSectionApplyTest,
	"UEREMCP.Animation.MontageSections.ApplyAndVerify",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMontageSectionApplyTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMontageSectionTest;

	UAnimMontage* Montage = MakeScratchMontage();
	if (!TestNotNull(TEXT("scratch montage created"), Montage))
	{
		return false;
	}
	SeedSection(Montage, TEXT("Doomed"), 0.f);

	TArray<FUeremcpMontageSectionSpec> Specs;
	{
		FUeremcpMontageSectionSpec Windup;
		Windup.Name = TEXT("Windup");
		Windup.StartTime = 0.f;
		Windup.NextSection = TEXT("Strike");
		Windup.bHasNextSection = true;
		Specs.Add(Windup);

		FUeremcpMontageSectionSpec Strike;
		Strike.Name = TEXT("Strike");
		Strike.StartTime = 0.35f;
		Specs.Add(Strike);
	}

	FUeremcpMontageSectionWriteResult Result;
	FString Error;
	if (!TestTrue(
			TEXT("apply succeeds"),
			FUeremcpMontageSectionWriter::Apply(Montage, Specs, /*bSave=*/false, Result, Error)))
	{
		AddError(FString::Printf(TEXT("apply error: %s"), *Error));
		return false;
	}

	TestTrue(TEXT("apply reports applied"), Result.bApplied);
	TestTrue(TEXT("post-write re-read verified"), Result.bRereadVerified);
	TestEqual(TEXT("no verification failures"), Result.VerificationFailures.Num(), 0);

	// Complete-state replacement: the unlisted section is gone.
	TestEqual(TEXT("montage has exactly the submitted sections"), Montage->CompositeSections.Num(), 2);
	TestEqual(
		TEXT("unlisted section was removed"),
		Montage->GetSectionIndex(FName(TEXT("Doomed"))),
		INDEX_NONE);

	const int32 WindupIndex = Montage->GetSectionIndex(FName(TEXT("Windup")));
	const int32 StrikeIndex = Montage->GetSectionIndex(FName(TEXT("Strike")));
	if (!TestTrue(TEXT("both submitted sections exist"), WindupIndex != INDEX_NONE && StrikeIndex != INDEX_NONE))
	{
		return false;
	}

	TestEqual(
		TEXT("Windup chains to Strike"),
		Montage->CompositeSections[WindupIndex].NextSectionName,
		FName(TEXT("Strike")));
	TestTrue(
		TEXT("Strike does not chain"),
		Montage->CompositeSections[StrikeIndex].NextSectionName.IsNone());
	TestTrue(
		TEXT("Strike start time applied"),
		FMath::IsNearlyEqual(
			Montage->CompositeSections[StrikeIndex].GetTime(EAnimLinkMethod::Absolute),
			0.35f,
			0.001f));

	// Re-applying the same state is a no-op plan.
	{
		FUeremcpMontageSectionPlan Plan;
		FUeremcpMontageSectionWriter::BuildPlan(Montage, Specs, Plan);
		TestFalse(TEXT("re-applying identical state reports no changes"), Plan.HasChanges());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMontageSectionEnvelopeTest,
	"UEREMCP.Animation.MontageSections.Envelope",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpMontageSectionEnvelopeTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpMontageSectionTest;

	// Wrong action.
	{
		TSharedPtr<FJsonObject> Response = Parse(
			UUeremcpAnimationToolset::SubmitMontageSections(
				TEXT(R"({"protocol_version":"1.0","action":"inspect_montage","target":{"asset_path":"/Game/X"},"specification":{}})")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("wrong action is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	// Missing sections array.
	{
		TSharedPtr<FJsonObject> Response = Parse(
			UUeremcpAnimationToolset::SubmitMontageSections(
				TEXT(R"({"protocol_version":"1.0","action":"submit_montage_sections","target":{"asset_path":"/Game/X"},"specification":{}})")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("missing sections is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	// Missing target.
	{
		TSharedPtr<FJsonObject> Response = Parse(
			UUeremcpAnimationToolset::SubmitMontageSections(
				TEXT(R"({"protocol_version":"1.0","action":"submit_montage_sections","specification":{"sections":[{"name":"A"}]}})")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("missing target is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	// Malformed envelope.
	{
		TSharedPtr<FJsonObject> Response = Parse(
			UUeremcpAnimationToolset::SubmitMontageSections(TEXT("{not json")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("malformed envelope is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
