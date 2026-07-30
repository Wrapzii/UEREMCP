// WS-11 contract regressions for the agent-facing visual capture tool.
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpVisualCaptureToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpVisualCaptureTests
{
	bool ParseResponse(const FString& Json, TSharedPtr<FJsonObject>& Out)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpVisualCaptureDryRunTest,
	"UEREMCP.Validation.VisualCapture.DryRunIsNonMutating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpVisualCaptureDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"visual-dry-run",
		"action":"capture_effect_frames",
		"target":{"asset_path":"/Game/__UeremcpPoc/NS_VisualProbe"},
		"options":{"dry_run":true}
	})");
	TSharedPtr<FJsonObject> Response;
	TestTrue(TEXT("response parses"), UeremcpVisualCaptureTests::ParseResponse(
		UUeremcpVisualCaptureToolset::CaptureEffectFrames(Request), Response));
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("dry run is partial"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestTrue(TEXT("summary says no capture"),
		Response->GetStringField(TEXT("summary")).Contains(TEXT("Dry run")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpVisualCaptureRejectsWrongActionTest,
	"UEREMCP.Validation.VisualCapture.RejectsWrongAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpVisualCaptureRejectsWrongActionTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"visual-wrong-action",
		"action":"create_niagara_effect",
		"target":{"asset_path":"/Game/__UeremcpPoc/NS_VisualProbe"}
	})");
	TSharedPtr<FJsonObject> Response;
	TestTrue(TEXT("response parses"), UeremcpVisualCaptureTests::ParseResponse(
		UUeremcpVisualCaptureToolset::CaptureEffectFrames(Request), Response));
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("wrong action rejected"),
		Response->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpVisualCaptureRejectsUnsafeDimensionsTest,
	"UEREMCP.Validation.VisualCapture.RejectsUnsafeDimensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpVisualCaptureRejectsUnsafeDimensionsTest::RunTest(
	const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"visual-invalid-size",
		"action":"capture_effect_frames",
		"target":{"asset_path":"/Game/__UeremcpPoc/NS_VisualProbe"},
		"specification":{"width":32000,"height":-1}
	})");
	TSharedPtr<FJsonObject> Response;
	TestTrue(TEXT("response parses"), UeremcpVisualCaptureTests::ParseResponse(
		UUeremcpVisualCaptureToolset::CaptureEffectFrames(Request), Response));
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("unsafe dimensions rejected"),
		Response->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

#endif
