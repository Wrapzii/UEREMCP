#include "Misc/AutomationTest.h"
#include "UeremcpNoise.h"
#include "UeremcpSpline.h"
#include "UeremcpEnvironmentService.h"
#include "UeremcpEnvironmentToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentNoiseDeterminismTest,
	"UEREMCP.Environment.Noise.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentNoiseDeterminismTest::RunTest(const FString& Parameters)
{
	const float A = UeremcpNoise::FBm2D(42, 1.25f, 3.5f, 4, 2.f, 0.5f);
	const float B = UeremcpNoise::FBm2D(42, 1.25f, 3.5f, 4, 2.f, 0.5f);
	const float C = UeremcpNoise::FBm2D(43, 1.25f, 3.5f, 4, 2.f, 0.5f);
	TestEqual(TEXT("same seed same noise"), A, B);
	TestTrue(TEXT("different seed differs"), !FMath::IsNearlyEqual(A, C));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentHeightmapNonFlatTest,
	"UEREMCP.Environment.Heightmap.NonFlatValley",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentHeightmapNonFlatTest::RunTest(const FString& Parameters)
{
	FUeremcpEnvironmentBuildSpec Spec;
	Spec.Seed = 7;
	Spec.SizeX = 127;
	Spec.SizeY = 127;
	const FBox Extents(FVector(0, 0, 0), FVector(12600, 12600, 0));
	const FUeremcpSplinePath River = UeremcpSpline::MakeRiverAcross(Spec.Seed, Extents, 10, 600.f);
	TArray<uint16> Heights;
	TSharedPtr<FJsonObject> Metrics;
	FUeremcpEnvironmentService::GenerateHeightmap(Spec, River, Heights, Metrics);
	TestTrue(TEXT("metrics"), Metrics.IsValid());
	TestTrue(TEXT("non_flat"), Metrics->GetBoolField(TEXT("non_flat")));
	TestTrue(TEXT("valley samples"), Metrics->GetNumberField(TEXT("valley_samples")) > 0);
	TestEqual(TEXT("height count"), Heights.Num(), Spec.SizeX * Spec.SizeY);
	const FString Hash1 = Metrics->GetStringField(TEXT("heightmap_hash"));
	TArray<uint16> Heights2;
	TSharedPtr<FJsonObject> Metrics2;
	FUeremcpEnvironmentService::GenerateHeightmap(Spec, River, Heights2, Metrics2);
	TestEqual(TEXT("heightmap_hash stable"), Hash1, Metrics2->GetStringField(TEXT("heightmap_hash")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentBuildDryRunTest,
	"UEREMCP.Environment.Build.DryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentBuildDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"build_environment",
		"request_id":"env-dry-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain"},
		"options":{"dry_run":true,"validate":true},
		"specification":{"seed":42,"include":{"capture":false}}
	})");
	const FString Json = UUeremcpEnvironmentToolset::BuildEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("no_change_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentRejectsWrongActionTest,
	"UEREMCP.Environment.Build.RejectsWrongAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentRejectsWrongActionTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"create_vfx_material",
		"request_id":"env-bad-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/X"},
		"options":{"dry_run":true},
		"specification":{"seed":1}
	})");
	const FString Json = UUeremcpEnvironmentToolset::BuildEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
	TestTrue(TEXT("has notes"), Root->TryGetArrayField(TEXT("capability_notes"), Notes) && Notes && Notes->Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentParseAliasesTest,
	"UEREMCP.Environment.Spec.ParseAliases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentParseAliasesTest::RunTest(const FString& Parameters)
{
	const FString SpecJson = TEXT(R"({
		"seed":99,
		"terrain":{"size":63,"z_scale":250,"mountain_weight":0.4},
		"structures":{"count":4},
		"capture":{"enabled":false}
	})");
	TSharedPtr<FJsonObject> SpecObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	TestTrue(TEXT("parse spec"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestTrue(TEXT("ParseBuildSpec"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestEqual(TEXT("size alias"), Spec.SizeX, 63);
	TestEqual(TEXT("size alias y"), Spec.SizeY, 63);
	TestEqual(TEXT("z_scale alias"), Spec.ScaleZ, 250.f);
	TestEqual(TEXT("mountain_weight alias"), Spec.MountainAmplitude, 0.4f);
	TestTrue(TEXT("structures include"), Spec.bIncludeStructures);
	TestEqual(TEXT("structure count"), Spec.StructureCount, 4);
	TestFalse(TEXT("capture off"), Spec.bCaptureScreenshot);
	return true;
}
