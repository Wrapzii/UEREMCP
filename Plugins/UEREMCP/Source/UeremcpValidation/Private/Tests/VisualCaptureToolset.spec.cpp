// WS-11 contract regressions for general visual capture + Niagara fail-soft gate.
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpNiagaraToolset.h"
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
		"options":{"dry_run":true,"validate":true}
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
		"target":{"asset_path":"/Game/__UeremcpPoc/NS_VisualProbe"},
		"options":{"validate":true}
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
		"options":{"validate":true},
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpWorldCaptureDryRunTest,
	"UEREMCP.Validation.VisualCapture.WorldDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpWorldCaptureDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"world-dry-run",
		"action":"capture_world_frames",
		"options":{"dry_run":true}
	})");
	TSharedPtr<FJsonObject> Response;
	TestTrue(TEXT("response parses"), UeremcpVisualCaptureTests::ParseResponse(
		UUeremcpVisualCaptureToolset::CaptureWorldFrames(Request), Response));
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("dry run status"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("no_change_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCaptureDryRunTest,
	"UEREMCP.Validation.VisualCapture.MaterialDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpMaterialCaptureDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"material-dry-run",
		"action":"capture_material_frames",
		"target":{"asset_path":"/Engine/BasicShapes/BasicShapeMaterial"},
		"options":{"dry_run":true}
	})");
	TSharedPtr<FJsonObject> Response;
	TestTrue(TEXT("response parses"), UeremcpVisualCaptureTests::ParseResponse(
		UUeremcpVisualCaptureToolset::CaptureMaterialFrames(Request), Response));
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("dry run status"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("no_change_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpAnimationCaptureRejectsMissingMeshTest,
	"UEREMCP.Validation.VisualCapture.AnimationRejectsMissingMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpAnimationCaptureRejectsMissingMeshTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"anim-missing-mesh",
		"action":"capture_animation_frames",
		"target":{"asset_path":"/Game/__UeremcpTests/MissingAnim"},
		"options":{"dry_run":false}
	})");
	TSharedPtr<FJsonObject> Response;
	TestTrue(TEXT("response parses"), UeremcpVisualCaptureTests::ParseResponse(
		UUeremcpVisualCaptureToolset::CaptureAnimationFrames(Request), Response));
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("missing anim rejected or failed soft"),
		Response->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraInspectMissingFailsSoftTest,
	"UEREMCP.Validation.Niagara.GetSystemSummaryFailSoft.MissingAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpNiagaraInspectMissingFailsSoftTest::RunTest(const FString& Parameters)
{
	// Regression for BACKLOG 3.3: inspect must fail soft (no editor crash) when
	// GetSystemSummary would be unsafe / target is missing.
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"ws11-inspect-missing",
		"action":"inspect_system",
		"target":{"asset_path":"/Game/__UeremcpTests/NS_DoesNotExist_WS11"},
		"options":{"response_detail":"diagnostic"}
	})");
	TSharedPtr<FJsonObject> Response;
	const FString Json = UUeremcpNiagaraToolset::InspectSystem(Request);
	TestTrue(TEXT("inspect response parses"),
		UeremcpVisualCaptureTests::ParseResponse(Json, Response));
	if (!Response.IsValid())
	{
		return false;
	}
	const FString Status = Response->GetStringField(TEXT("status"));
	TestTrue(
		TEXT("missing asset fails soft (rejected/failed/error — never silent ok)"),
		Status == TEXT("rejected")
			|| Status == TEXT("failed_validation")
			|| Status == TEXT("error")
			|| Status == TEXT("partially_completed"));
	TestFalse(TEXT("must not claim validated"),
		Status.Contains(TEXT("validated")));
	return true;
}

#endif
