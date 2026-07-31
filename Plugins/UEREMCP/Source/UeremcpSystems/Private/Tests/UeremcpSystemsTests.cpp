#include "Misc/AutomationTest.h"
#include "UeremcpAudioService.h"
#include "UeremcpNetworkingService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSystemsPatternBTest,
	"Ueremcp.Systems.Networking.PatternBChecklist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpSystemsPatternBTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestTrue(TEXT("valid Pattern B"), FUeremcpNetworkingService::CheckPatternB(
		TEXT("B"), TEXT("server"), TEXT("AuthorityCastAbility"), Error));
	TestFalse(TEXT("bad pattern"), FUeremcpNetworkingService::CheckPatternB(
		TEXT("A"), TEXT("server"), TEXT("AuthorityCastAbility"), Error));
	TestTrue(TEXT("error names Pattern B"), Error.Contains(TEXT("Pattern B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSystemsAudioSpecTest,
	"Ueremcp.Systems.Audio.ParseCreateSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpSystemsAudioSpecTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Waves;
	Waves.Add(MakeShared<FJsonValueString>(TEXT("/Game/Audio/SW_Test")));
	Spec->SetArrayField(TEXT("sound_waves"), Waves);

	FUeremcpAudioCuePlan Plan;
	FString Error;
	TestFalse(
		TEXT("rejects non-scratch target"),
		FUeremcpAudioService::ParseCreateSpec(Spec, TEXT("/Game/Audio/SC_Bad"), Plan, Error));
	TestTrue(
		TEXT("accepts scratch target"),
		FUeremcpAudioService::ParseCreateSpec(
			Spec, TEXT("/Game/__UeremcpTests/Audio/SC_Cast"), Plan, Error));
	TestEqual(TEXT("one wave"), Plan.SoundWavePaths.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSystemsReplicationSpecTest,
	"Ueremcp.Systems.Networking.ParseValidateSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpSystemsReplicationSpecTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Networking = MakeShared<FJsonObject>();
	Networking->SetStringField(TEXT("pattern"), TEXT("B"));
	Networking->SetStringField(TEXT("authority"), TEXT("server"));
	Networking->SetStringField(TEXT("cast_path"), TEXT("AuthorityCastAbility"));
	Spec->SetObjectField(TEXT("networking"), Networking);

	TArray<FUeremcpReplicationExpectation> Expectations;
	bool bRequirePatternB = false;
	bool bApplyFixes = false;
	FString Pattern, Authority, CastPath, Error;
	TestTrue(
		TEXT("parse networking-only"),
		FUeremcpNetworkingService::ParseValidateSpec(
			Spec, Expectations, bRequirePatternB, Pattern, Authority, CastPath, bApplyFixes, Error));
	TestTrue(TEXT("requires Pattern B"), bRequirePatternB);
	TestEqual(TEXT("pattern"), Pattern, FString(TEXT("B")));
	return true;
}
