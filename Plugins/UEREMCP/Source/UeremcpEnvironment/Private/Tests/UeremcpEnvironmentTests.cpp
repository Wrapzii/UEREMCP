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
		"specification":{"seed":42,"include":{"terrain":true,"river":true,"forest":true,"rain":true,"lighting":true,"capture":false},"fallback_policy":"allow_approximate"}
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
	TestEqual(TEXT("legacy structure count"), Spec.Structures[0].Count, 4);
	TestFalse(TEXT("capture off"), Spec.bCaptureScreenshot);
	TestFalse(TEXT("terrain opt-in default"), Spec.bIncludeTerrain);
	TestFalse(TEXT("river opt-in default"), Spec.bIncludeRiver);
	TestFalse(TEXT("forest opt-in default"), Spec.bIncludeForest);
	TestFalse(TEXT("rain opt-in default"), Spec.bIncludeRain);
	TestFalse(TEXT("lighting opt-in default"), Spec.bIncludeLighting);
	TestEqual(TEXT("missing river width keeps default"), Spec.RiverWidth, 600.f);
	TestEqual(TEXT("missing forest width keeps default"), Spec.ForestBankWidth, 3500.f);
	TestEqual(TEXT("default fallback policy"), Spec.FallbackPolicy, FString(TEXT("prefer_real")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentRainFallbackPolicyParseTest,
	"UEREMCP.Environment.Spec.RainFallbackPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentRainFallbackPolicyParseTest::RunTest(const FString& Parameters)
{
	const FString SpecJson = TEXT(R"({
		"seed":7,
		"fallback_policy":"allow_approximate",
		"weather":{"follow":"player_camera","streak_count":128},
		"include":{"rain":true,"terrain":false,"river":false,"forest":false,"lighting":false}
	})");
	TSharedPtr<FJsonObject> SpecObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestTrue(TEXT("ParseBuildSpec"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestEqual(TEXT("opt-in approximate"), Spec.FallbackPolicy, FString(TEXT("allow_approximate")));
	TestEqual(TEXT("streak count"), Spec.RainStreakCount, 128);
	TestTrue(TEXT("rain included"), Spec.bIncludeRain);
	TestFalse(TEXT("terrain excluded"), Spec.bIncludeTerrain);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentFallbackPolicyParseTest,
	"UEREMCP.Environment.Spec.FallbackPolicyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentFallbackPolicyParseTest::RunTest(const FString& Parameters)
{
	const FString SpecJson = TEXT(R"({"seed":1,"fallback_policy":"allow_approximate"})");
	TSharedPtr<FJsonObject> SpecObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	TestTrue(TEXT("parse spec"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestTrue(TEXT("valid policy"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestEqual(TEXT("policy stored"), Spec.FallbackPolicy, FString(TEXT("allow_approximate")));

	const FString BadJson = TEXT(R"({"seed":1,"fallback_policy":"silent_ok"})");
	TSharedPtr<FJsonObject> BadObj;
	const TSharedRef<TJsonReader<>> BadReader = TJsonReaderFactory<>::Create(BadJson);
	TestTrue(TEXT("parse bad"), FJsonSerializer::Deserialize(BadReader, BadObj) && BadObj.IsValid());
	TestFalse(TEXT("reject unknown policy"), FUeremcpEnvironmentService::ParseBuildSpec(BadObj, Spec, Err));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentV2SnowIceHailParseTest,
	"UEREMCP.Environment.Spec.V2SnowIceHail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentV2SnowIceHailParseTest::RunTest(const FString& Parameters)
{
	const FString SpecJson = TEXT(R"({
		"schema_version":2,
		"seed":8801,
		"terrain":{"profile":"mountains","mountain_amplitude":0.62},
		"weather":[
			{"phenomenon":"snow","intensity":0.75,"follow_player":true},
			{"phenomenon":"hail","intensity":0.35}
		],
		"structures":[{"kind":"ice_wall_ring","count":28,"height":900}],
		"lighting":{"preset":"blizzard"}
	})");
	TSharedPtr<FJsonObject> SpecObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	TestTrue(TEXT("parse spec"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestTrue(TEXT("ParseBuildSpec v2"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestTrue(TEXT("terrain presence"), Spec.bIncludeTerrain);
	TestEqual(TEXT("terrain profile"), Spec.TerrainProfile, EUeremcpTerrainProfile::Mountains);
	TestEqual(TEXT("weather count"), Spec.WeatherPhenomena.Num(), 2);
	TestEqual(TEXT("snow phenomenon"), Spec.WeatherPhenomena[0].Phenomenon, FString(TEXT("snow")));
	TestEqual(TEXT("hail phenomenon"), Spec.WeatherPhenomena[1].Phenomenon, FString(TEXT("hail")));
	TestEqual(TEXT("structures count"), Spec.Structures.Num(), 1);
	TestEqual(TEXT("ice wall kind"), Spec.Structures[0].Kind, FString(TEXT("ice_wall_ring")));
	TestTrue(TEXT("lighting presence"), Spec.bIncludeLighting);
	TestEqual(TEXT("blizzard preset"), Spec.LightingPreset, FString(TEXT("blizzard")));
	TestFalse(TEXT("river not implied"), Spec.bIncludeRiver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentV2SnowDryRunTest,
	"UEREMCP.Environment.Build.V2SnowIceHailDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentV2SnowDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"build_environment",
		"request_id":"env-snow-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/SnowIceHail"},
		"options":{"dry_run":true,"validate":true},
		"specification":{
			"schema_version":2,
			"seed":8801,
			"terrain":{"profile":"mountains","mountain_amplitude":0.62},
			"weather":[
				{"phenomenon":"snow","intensity":0.75},
				{"phenomenon":"hail","intensity":0.35}
			],
			"structures":[{"kind":"ice_wall_ring","count":12,"height":600}],
			"lighting":{"preset":"blizzard"}
		}
	})");
	const FString Json = UUeremcpEnvironmentToolset::BuildEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(JsonReader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("no_change_required")));
	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	TestTrue(TEXT("metrics"), Root->TryGetObjectField(TEXT("structural_metrics"), Metrics) && Metrics);
	TestTrue(TEXT("non_flat planned"), (*Metrics)->GetBoolField(TEXT("non_flat")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentPreferRealRejectsFallbackDryRunTest,
	"UEREMCP.Environment.Build.RejectsPreferRealFallbackDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentPreferRealRejectsFallbackDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"build_environment",
		"request_id":"env-fallback-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain"},
		"options":{"dry_run":true,"validate":true},
		"specification":{"seed":42,"fallback_policy":"prefer_real","include":{"terrain":true,"river":true,"forest":true,"rain":true,"lighting":false,"capture":false}}
	})");
	const FString Json = UUeremcpEnvironmentToolset::BuildEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentAllowApproximateFlagsDryRunTest,
	"UEREMCP.Environment.Build.AllowApproximateFlagsDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentAllowApproximateFlagsDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"build_environment",
		"request_id":"env-fallback-2",
		"target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain"},
		"options":{"dry_run":true,"validate":true},
		"specification":{"seed":42,"fallback_policy":"allow_approximate","include":{"terrain":true,"river":true,"forest":true,"rain":true,"lighting":false,"capture":false}}
	})");
	const FString Json = UUeremcpEnvironmentToolset::BuildEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("no_change_required")));
	TestTrue(TEXT("approximated flagged"), Root->GetBoolField(TEXT("approximated")));
	const TSharedPtr<FJsonObject>* RealVsApprox = nullptr;
	TestTrue(TEXT("real_vs_approximated"),
		Root->TryGetObjectField(TEXT("real_vs_approximated"), RealVsApprox)
			&& RealVsApprox && RealVsApprox->IsValid());
	TestTrue(TEXT("real_vs_approximated.approximated"), (*RealVsApprox)->GetBoolField(TEXT("approximated")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentSplineBankSideTest,
	"UEREMCP.Environment.Spline.OppositeBanks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentSplineBankSideTest::RunTest(const FString& Parameters)
{
	FUeremcpSplinePath Path;
	Path.Points.Add({ FVector(0, 0, 0), 500.f });
	Path.Points.Add({ FVector(1000, 0, 0), 500.f });
	const float SideA = Path.SignedSideToClosestXY(FVector(500, 250, 0));
	const float SideB = Path.SignedSideToClosestXY(FVector(500, -250, 0));
	TestTrue(TEXT("positive bank"), SideA > 0.f);
	TestTrue(TEXT("negative bank"), SideB < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentReadOnlyNoSeedTest,
	"UEREMCP.Environment.Inspect.DoesNotRequireSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentReadOnlyNoSeedTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"inspect_environment",
		"request_id":"env-inspect-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain"},
		"specification":{}
	})");
	const FString Json = UUeremcpEnvironmentToolset::InspectEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestNotEqual(TEXT("not rejected for absent seed"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentMinimalSpecOptInTest,
	"UEREMCP.Environment.Spec.MinimalOptInDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentMinimalSpecOptInTest::RunTest(const FString& Parameters)
{
	const FString SpecJson = TEXT(R"({"seed":1})");
	TSharedPtr<FJsonObject> SpecObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	TestTrue(TEXT("parse spec"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestTrue(TEXT("ParseBuildSpec"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestFalse(TEXT("terrain default false"), Spec.bIncludeTerrain);
	TestFalse(TEXT("river default false"), Spec.bIncludeRiver);
	TestFalse(TEXT("forest default false"), Spec.bIncludeForest);
	TestFalse(TEXT("rain default false"), Spec.bIncludeRain);
	TestFalse(TEXT("lighting default false"), Spec.bIncludeLighting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentRainOnlyIncludeTest,
	"UEREMCP.Environment.Spec.RainOnlyInclude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentRainOnlyIncludeTest::RunTest(const FString& Parameters)
{
	const FString SpecJson = TEXT(R"({
		"seed":99,
		"include":{"rain":true},
		"weather":{"follow":"player_camera"},
		"fallback_policy":"allow_approximate"
	})");
	TSharedPtr<FJsonObject> SpecObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	TestTrue(TEXT("parse spec"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestTrue(TEXT("ParseBuildSpec rain-only"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestTrue(TEXT("rain included"), Spec.bIncludeRain);
	TestFalse(TEXT("forest not forced"), Spec.bIncludeForest);
	TestFalse(TEXT("terrain not forced"), Spec.bIncludeTerrain);
	TestFalse(TEXT("river not forced"), Spec.bIncludeRiver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentIncludeDependencyTest,
	"UEREMCP.Environment.Spec.IncludeDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentIncludeDependencyTest::RunTest(const FString& Parameters)
{
	const FString ForestOnly = TEXT(R"({"seed":1,"include":{"forest":true}})");
	TSharedPtr<FJsonObject> SpecObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ForestOnly);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	FUeremcpEnvironmentBuildSpec Spec;
	FString Err;
	TestFalse(TEXT("forest without river rejected"), FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestTrue(TEXT("error mentions river"), Err.Contains(TEXT("river")));

	const FString RainPreferReal = TEXT(R"({"seed":1,"include":{"rain":true}})");
	Reader = TJsonReaderFactory<>::Create(RainPreferReal);
	SpecObj.Reset();
	TestTrue(TEXT("parse rain"), FJsonSerializer::Deserialize(Reader, SpecObj) && SpecObj.IsValid());
	Err.Reset();
	TestTrue(TEXT("rain prefer_real parses (CreateNiagaraEffect at build)"),
		FUeremcpEnvironmentService::ParseBuildSpec(SpecObj, Spec, Err));
	TestEqual(TEXT("rain phenomenon added"), Spec.WeatherPhenomena.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentMinimalDryRunTest,
	"UEREMCP.Environment.Build.MinimalDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentMinimalDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"build_environment",
		"request_id":"env-minimal-1",
		"target":{"asset_path":"/Game/__UeremcpTests/OptInShell"},
		"options":{"dry_run":true},
		"specification":{"seed":1}
	})");
	const FString Json = UUeremcpEnvironmentToolset::BuildEnvironment(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(JsonReader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("no_change_required")));
	const FString Summary = Root->GetStringField(TEXT("summary"));
	TestTrue(TEXT("summary mentions opt-in"), Summary.Contains(TEXT("no include")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentNextArgsNonuniformScaleTest,
	"UEREMCP.Environment.Spec.NextArgsNonuniformScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentNextArgsNonuniformScaleTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"create_landscape",
		"request_id":"scale-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/ScaleGate"},
		"options":{"dry_run":true},
		"specification":{"seed":42,"terrain":{"profile":"mountains","scale_xy":300,"scale_z":100}}
	})");
	const FString Json = UUeremcpEnvironmentToolset::CreateLandscape(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	const TSharedPtr<FJsonObject>* Error = nullptr;
	TestTrue(TEXT("error object"), Root->TryGetObjectField(TEXT("error"), Error) && Error);
	TestEqual(TEXT("code"), (*Error)->GetStringField(TEXT("code")), FString(TEXT("NONUNIFORM_SCALE")));
	const TSharedPtr<FJsonObject>* NextArgs = nullptr;
	TestTrue(TEXT("next_args"), (*Error)->TryGetObjectField(TEXT("next_args"), NextArgs) && NextArgs);
	const TSharedPtr<FJsonObject>* Spec = nullptr;
	TestTrue(TEXT("spec patch"), (*NextArgs)->TryGetObjectField(TEXT("specification"), Spec) && Spec);
	const TSharedPtr<FJsonObject>* Terrain = nullptr;
	TestTrue(TEXT("terrain patch"), (*Spec)->TryGetObjectField(TEXT("terrain"), Terrain) && Terrain);
	TestEqual(TEXT("scale_xy recipe"), (*Terrain)->GetNumberField(TEXT("scale_xy")), 3.0);
	TestEqual(TEXT("scale_z recipe"), (*Terrain)->GetNumberField(TEXT("scale_z")), 3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpEnvironmentNextArgsNeedleScaleTest,
	"UEREMCP.Environment.Spec.NextArgsNeedleScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpEnvironmentNextArgsNeedleScaleTest::RunTest(const FString& Parameters)
{
	// NEEDLE alone used to patch only scale_z → merge left scale_xy=100 and
	// the next call failed NONUNIFORM (3 RTT). Both axes must be in next_args.
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"create_landscape",
		"request_id":"scale-needle-1",
		"target":{"asset_path":"/Game/__UeremcpPoc/ScaleGate"},
		"options":{"dry_run":true},
		"specification":{"seed":42,"terrain":{"profile":"mountains","scale_xy":100,"scale_z":100}}
	})");
	const FString Json = UUeremcpEnvironmentToolset::CreateLandscape(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	const TSharedPtr<FJsonObject>* Error = nullptr;
	TestTrue(TEXT("error object"), Root->TryGetObjectField(TEXT("error"), Error) && Error);
	TestEqual(TEXT("code"), (*Error)->GetStringField(TEXT("code")), FString(TEXT("NEEDLE_SCALE_Z")));
	const TSharedPtr<FJsonObject>* NextArgs = nullptr;
	TestTrue(TEXT("next_args"), (*Error)->TryGetObjectField(TEXT("next_args"), NextArgs) && NextArgs);
	const TSharedPtr<FJsonObject>* Spec = nullptr;
	TestTrue(TEXT("spec patch"), (*NextArgs)->TryGetObjectField(TEXT("specification"), Spec) && Spec);
	const TSharedPtr<FJsonObject>* Terrain = nullptr;
	TestTrue(TEXT("terrain patch"), (*Spec)->TryGetObjectField(TEXT("terrain"), Terrain) && Terrain);
	TestEqual(TEXT("scale_xy recipe"), (*Terrain)->GetNumberField(TEXT("scale_xy")), 3.0);
	TestEqual(TEXT("scale_z recipe"), (*Terrain)->GetNumberField(TEXT("scale_z")), 3.0);
	TestTrue(TEXT("has scale_xy field"), (*Terrain)->HasField(TEXT("scale_xy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpFindProjectAssetsRejectsEmptyRolesTest,
	"UEREMCP.Environment.FindProjectAssets.RejectsEmptyRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpFindProjectAssetsRejectsEmptyRolesTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"find_project_assets",
		"request_id":"fa-empty",
		"specification":{}
	})");
	const FString Json = UUeremcpEnvironmentToolset::FindProjectAssets(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlacePrefabDryRunTest,
	"UEREMCP.Environment.PlacePrefab.DryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeremcpPlacePrefabDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(R"({
		"protocol_version":"1.0",
		"action":"place_prefab_on_landscape",
		"request_id":"pp-dry",
		"options":{"dry_run":true},
		"specification":{
			"mesh_path":"/Engine/BasicShapes/Cube",
			"location_xy":[0,0],
			"clear_foliage_radius_cm":500
		}
	})");
	const FString Json = UUeremcpEnvironmentToolset::PlacePrefabOnLandscape(Request);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("parse"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	TestEqual(TEXT("status"), Root->GetStringField(TEXT("status")), FString(TEXT("no_change_required")));
	return true;
}
