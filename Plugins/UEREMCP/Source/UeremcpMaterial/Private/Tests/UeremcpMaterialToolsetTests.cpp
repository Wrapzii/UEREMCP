// Editor automation tests for UeremcpMaterial toolset (WS-08).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Materials/Material.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialFeatures.h"
#include "UeremcpMaterialNiagaraExport.h"
#include "UeremcpMaterialPaths.h"
#include "UeremcpMaterialToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpMaterialTests
{
	static UEditorAssetSubsystem* GetAssetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	}

	static void DeleteIfExists(const FString& PackagePath)
	{
		UEditorAssetSubsystem* Subsystem = GetAssetSubsystem();
		if (Subsystem && Subsystem->DoesAssetExist(PackagePath))
		{
			Subsystem->DeleteAsset(PackagePath);
		}
	}

	static void CleanupWs08MaterialScratch()
	{
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Materials/MI_WS08_ProjectileCore_Fire"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Materials/MI_WS08_ProjectileTrail_Ice"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Materials/MI_NS_WS08_ExportProbe_core"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Materials/MI_WS08_ValidateFalse_Core"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Materials/MI_WS08_Distortion_Probe"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Materials/MI_WS08_Flipbook_Probe"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Textures/T_WS08_ValidateFalse_Noise"));
		DeleteIfExists(TEXT("/Game/__UeremcpTests/Textures/T_MI_WS08_ProjectileTrail_Ice_FlowMap_flow_map"));

		UEditorAssetSubsystem* Subsystem = GetAssetSubsystem();
		if (!Subsystem)
		{
			return;
		}
		const TArray<FString> Masters = Subsystem->ListAssets(UeremcpMaterialPaths::MastersFolder);
		for (const FString& AssetPath : Masters)
		{
			const FString AssetName = FPaths::GetBaseFilename(AssetPath);
			if (AssetName.StartsWith(TEXT("M_Ueremcp_")))
			{
				Subsystem->DeleteAsset(AssetPath);
			}
		}
	}

	static bool ParseStatus(const FString& Json, FString& OutStatus)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		return Root->TryGetStringField(TEXT("status"), OutStatus);
	}

	static void ExpectHonestValidatedCreateStatus(
		FAutomationTestBase* Test,
		const FString& Json,
		const FString& Status)
	{
		if (Status == TEXT("created_and_validated") || Status == TEXT("modified_and_validated"))
		{
			Test->TestTrue(TEXT("validated status is honest *_validated"), true);
			return;
		}

		Test->TestEqual(TEXT("honest create status when proof unavailable"), Status, FString(TEXT("partially_completed")));
		Test->TestFalse(
			TEXT("must not claim *_validated without proof"),
			Json.Contains(TEXT("created_and_validated")) || Json.Contains(TEXT("modified_and_validated")));
	}

	static void ExpectDiskAssetWhenValidated(
		FAutomationTestBase* Test,
		UEditorAssetSubsystem* Subsystem,
		const FString& Target,
		const FString& Status)
	{
		if (Status != TEXT("created_and_validated") && Status != TEXT("modified_and_validated"))
		{
			return;
		}

		Test->TestTrue(TEXT("MI asset exists on disk when validated"), Subsystem && Subsystem->DoesAssetExist(Target));
	}

	static int32 ReadTextureDimensionX(const UTexture2D* Texture)
	{
		if (!Texture)
		{
			return 0;
		}
		if (Texture->Source.IsValid() && Texture->Source.GetSizeX() > 0)
		{
			return Texture->Source.GetSizeX();
		}
		return Texture->GetSizeX();
	}

	static int32 ReadTextureDimensionY(const UTexture2D* Texture)
	{
		if (!Texture)
		{
			return 0;
		}
		if (Texture->Source.IsValid() && Texture->Source.GetSizeY() > 0)
		{
			return Texture->Source.GetSizeY();
		}
		return Texture->GetSizeY();
	}

	static FString FindDependencyPath(const FString& Json, const FString& Role)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return FString();
		}

		const TArray<TSharedPtr<FJsonValue>>* Deps = nullptr;
		if (!Root->TryGetArrayField(TEXT("dependencies"), Deps) || !Deps)
		{
			return FString();
		}

		for (const TSharedPtr<FJsonValue>& Value : *Deps)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
			{
				continue;
			}

			FString DepRole;
			if ((*ObjPtr)->TryGetStringField(TEXT("role"), DepRole) && DepRole == Role)
			{
				FString Path;
				if ((*ObjPtr)->TryGetStringField(TEXT("asset_path"), Path))
				{
					return Path;
				}
			}
		}

		return FString();
	}

	static bool VerifyMasterFeatureWired(
		FAutomationTestBase* Test,
		const FString& MasterPath,
		const TArray<FString>& Features,
		const FString& FeatureToken)
	{
		UEditorAssetSubsystem* Subsystem = GetAssetSubsystem();
		if (!Test->TestNotNull(TEXT("asset subsystem"), Subsystem))
		{
			return false;
		}

		UMaterial* Material = UeremcpMaterialAssetLoad::ResolveMaterial(MasterPath);
		if (!Test->TestNotNull(*FString::Printf(TEXT("master material loads: %s"), *MasterPath), Material))
		{
			return false;
		}

		UeremcpMaterialFeatures::FFeatureGraphVerifyResult Verify;
		const bool bGraphOk = UeremcpMaterialFeatures::VerifyFeatureGraph(Material, Features, Verify);
		Test->TestTrue(TEXT("VerifyFeatureGraph succeeds"), bGraphOk);

		const bool* bWired = Verify.FeatureWired.Find(FeatureToken);
		Test->TestTrue(
			*FString::Printf(TEXT("feature '%s' wired in master graph"), *FeatureToken),
			bWired != nullptr && *bWired);
		return bGraphOk && bWired && *bWired;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialToolsetEchoTest,
	"UeremcpMaterial.Toolset.Echo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialToolsetEchoTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"mat-echo-1","action":"create_vfx_material","target":{"asset_path":"/Game/__UeremcpTests/Materials/MI_Probe"}})");
	const FString Json = UUeremcpMaterialToolset::Echo(Request);

	FString Status;
	TestTrue(TEXT("Echo returns parseable status"), UeremcpMaterialTests::ParseStatus(Json, Status));
	TestEqual(TEXT("status"), Status, FString(TEXT("no_change_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialToolsetRegisterTest,
	"UeremcpMaterial.Toolset.Register",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialToolsetRegisterTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpMaterialToolset::StaticClass()))
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpMaterialToolset::StaticClass());
	}

	TestTrue(TEXT("toolset class registered"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpMaterialToolset::StaticClass()));

	const FString SchemaJson = UToolsetRegistry::GetToolsetJsonSchema(UUeremcpMaterialToolset::StaticClass());
	TestFalse(TEXT("schema non-empty"), SchemaJson.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateVfxProjectileCoreTest,
	"UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateVfxProjectileCoreTest::RunTest(const FString& Parameters)
{
	UeremcpMaterialTests::CleanupWs08MaterialScratch();

	const FString Target = TEXT("/Game/__UeremcpTests/Materials/MI_WS08_ProjectileCore_Fire");
	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-core-fire",
		"action":"create_vfx_material",
		"target":{"asset_path":"%s"},
		"specification":{
			"purpose":"elemental_projectile_core",
			"element":"fire",
			"modifiers":["boost_impact"],
			"features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]
		},
		"options":{"compile":true,"validate":true,"save":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateVfxMaterial(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(this, Json, Status);

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	TestNotNull(TEXT("asset subsystem"), Subsystem);
	if (Subsystem)
	{
		UeremcpMaterialTests::ExpectDiskAssetWhenValidated(this, Subsystem, Target, Status);
	}

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateVfxProjectileTrailTest,
	"UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateVfxProjectileTrailTest::RunTest(const FString& Parameters)
{
	UeremcpMaterialTests::CleanupWs08MaterialScratch();

	const FString Target = TEXT("/Game/__UeremcpTests/Materials/MI_WS08_ProjectileTrail_Ice");
	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-trail-ice",
		"action":"create_vfx_material",
		"target":{"asset_path":"%s"},
		"specification":{
			"purpose":"elemental_projectile_trail",
			"element":"ice",
			"modifiers":["crystalline_fragments","reduce_trail_persistence"],
			"features":["panning_textures","erosion","depth_fade","dynamic_color"],
			"textures":{"FlowMap":{"generate":"flow_map","dimensions":[256,256]}}
		},
		"options":{"compile":true,"validate":true,"save":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateVfxMaterial(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(this, Json, Status);

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		UeremcpMaterialTests::ExpectDiskAssetWhenValidated(this, Subsystem, Target, Status);
		if (Status == TEXT("created_and_validated") || Status == TEXT("modified_and_validated"))
		{
			const FString FlowMapPath =
				TEXT("/Game/__UeremcpTests/Textures/T_MI_WS08_ProjectileTrail_Ice_FlowMap_flow_map");
			TestTrue(TEXT("generated FlowMap texture exists"), Subsystem->DoesAssetExist(FlowMapPath));
		}
	}

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateProceduralTextureTest,
	"UeremcpMaterial.Toolset.CreateProceduralTexture.Noise",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateProceduralTextureTest::RunTest(const FString& Parameters)
{
	const FString Target = TEXT("/Game/__UeremcpTests/Textures/T_WS08_Noise_Probe");
	UeremcpMaterialTests::DeleteIfExists(Target);

	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-tex-noise",
		"action":"create_procedural_texture",
		"target":{"asset_path":"%s"},
		"specification":{"generate":"noise","dimensions":[128,128],"seed":7},
		"options":{"save":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateProceduralTexture(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(this, Json, Status);

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		TestTrue(TEXT("texture asset exists"), Subsystem->DoesAssetExist(Target));
	}

	UeremcpMaterialTests::DeleteIfExists(Target);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateProceduralTextureFlipbookAtlasTest,
	"UeremcpMaterial.Toolset.CreateProceduralTexture.FlipbookAtlas",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateProceduralTextureFlipbookAtlasTest::RunTest(const FString& Parameters)
{
	const FString Target = TEXT("/Game/__UeremcpTests/Textures/T_WS08_FlipbookAtlas_Probe");
	UeremcpMaterialTests::DeleteIfExists(Target);

	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-tex-flipbook",
		"action":"create_procedural_texture",
		"target":{"asset_path":"%s"},
		"specification":{
			"generate":"flipbook_atlas",
			"dimensions":[256,256],
			"flipbook":{"columns":4,"rows":4,"frame_count":16},
			"seed":11
		},
		"options":{"save":true,"validate":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateProceduralTexture(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(this, Json, Status);

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		TestTrue(TEXT("flipbook atlas texture exists"), Subsystem->DoesAssetExist(Target));
		if (UTexture2D* Texture = UeremcpMaterialAssetLoad::TryLoadTexture(Target))
		{
			TestEqual(TEXT("atlas width"), UeremcpMaterialTests::ReadTextureDimensionX(Texture), 256);
			TestEqual(TEXT("atlas height"), UeremcpMaterialTests::ReadTextureDimensionY(Texture), 256);
		}
	}

	UeremcpMaterialTests::DeleteIfExists(Target);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialNiagaraExportServiceTest,
	"UeremcpMaterial.Service.NiagaraExport.CoreMaterial",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialNiagaraExportServiceTest::RunTest(const FString& Parameters)
{
	UeremcpMaterialTests::CleanupWs08MaterialScratch();

	const TSharedPtr<FJsonObject> CreateSpec = MakeShared<FJsonObject>();
	CreateSpec->SetStringField(TEXT("element"), TEXT("fire"));
	CreateSpec->SetStringField(TEXT("purpose"), TEXT("elemental_projectile_core"));

	const FUeremcpMaterialCreateResult Result =
		UeremcpMaterialNiagaraExport::ExecuteCreateVfxMaterialForNiagaraRole(
			TEXT("NS_WS08_ExportProbe"),
			TEXT("core"),
			CreateSpec,
			true,
			true,
			true);

	TestTrue(TEXT("service bSuccess"), Result.bSuccess);
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(
		this,
		FString::Printf(TEXT("{\"status\":\"%s\"}"), *Result.Status),
		Result.Status);
	TestFalse(TEXT("PrimaryAsset set"), Result.PrimaryAsset.IsEmpty());

	FString VerifyError;
	TestTrue(
		TEXT("PrimaryAsset loads as UMaterialInterface"),
		UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface(Result.PrimaryAsset, VerifyError));

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		UeremcpMaterialTests::ExpectDiskAssetWhenValidated(this, Subsystem, Result.PrimaryAsset, Result.Status);
	}

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateVfxDistortionTest,
	"UeremcpMaterial.Toolset.CreateVfxMaterial.Distortion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateVfxDistortionTest::RunTest(const FString& Parameters)
{
	UeremcpMaterialTests::CleanupWs08MaterialScratch();

	const FString Target = TEXT("/Game/__UeremcpTests/Materials/MI_WS08_Distortion_Probe");
	const TArray<FString> Features = {
		TEXT("distortion"),
		TEXT("dynamic_color"),
	};
	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-distortion-probe",
		"action":"create_vfx_material",
		"target":{"asset_path":"%s"},
		"specification":{
			"purpose":"elemental_projectile_trail",
			"element":"fire",
			"features":["distortion","dynamic_color"]
		},
		"options":{"compile":true,"validate":true,"save":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateVfxMaterial(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(this, Json, Status);

	if (Status == TEXT("created_and_validated") || Status == TEXT("modified_and_validated"))
	{
		const FString MasterPath =
			UeremcpMaterialTests::FindDependencyPath(Json, TEXT("master_template"));
		TestFalse(TEXT("master dependency reported"), MasterPath.IsEmpty());
		UeremcpMaterialTests::VerifyMasterFeatureWired(this, MasterPath, Features, TEXT("distortion"));
	}

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		UeremcpMaterialTests::ExpectDiskAssetWhenValidated(this, Subsystem, Target, Status);
	}

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateVfxFlipbookSubuvTest,
	"UeremcpMaterial.Toolset.CreateVfxMaterial.FlipbookSubuv",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateVfxFlipbookSubuvTest::RunTest(const FString& Parameters)
{
	UeremcpMaterialTests::CleanupWs08MaterialScratch();

	const FString Target = TEXT("/Game/__UeremcpTests/Materials/MI_WS08_Flipbook_Probe");
	const TArray<FString> Features = {
		TEXT("flipbook_subuv"),
		TEXT("dynamic_color"),
	};
	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-flipbook-probe",
		"action":"create_vfx_material",
		"target":{"asset_path":"%s"},
		"specification":{
			"purpose":"elemental_projectile_trail",
			"element":"fire",
			"features":["flipbook_subuv","dynamic_color"]
		},
		"options":{"compile":true,"validate":true,"save":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateVfxMaterial(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	UeremcpMaterialTests::ExpectHonestValidatedCreateStatus(this, Json, Status);

	if (Status == TEXT("created_and_validated") || Status == TEXT("modified_and_validated"))
	{
		const FString MasterPath =
			UeremcpMaterialTests::FindDependencyPath(Json, TEXT("master_template"));
		TestFalse(TEXT("master dependency reported"), MasterPath.IsEmpty());
		UeremcpMaterialTests::VerifyMasterFeatureWired(this, MasterPath, Features, TEXT("flipbook_subuv"));
	}

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		UeremcpMaterialTests::ExpectDiskAssetWhenValidated(this, Subsystem, Target, Status);
	}

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateVfxValidateFalseTest,
	"UeremcpMaterial.Toolset.CreateVfxMaterial.ValidateFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateVfxValidateFalseTest::RunTest(const FString& Parameters)
{
	UeremcpMaterialTests::CleanupWs08MaterialScratch();

	const FString Target = TEXT("/Game/__UeremcpTests/Materials/MI_WS08_ValidateFalse_Core");
	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-validate-false",
		"action":"create_vfx_material",
		"target":{"asset_path":"%s"},
		"specification":{
			"purpose":"elemental_projectile_core",
			"element":"fire",
			"features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]
		},
		"options":{"compile":true,"validate":false,"save":true}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateVfxMaterial(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	TestEqual(TEXT("partially_completed when validate false"), Status, FString(TEXT("partially_completed")));
	TestFalse(TEXT("summary must not claim re-read verified"), Json.Contains(TEXT("re-read verified")));

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialCreateProceduralTextureValidateFalseTest,
	"UeremcpMaterial.Toolset.CreateProceduralTexture.ValidateFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialCreateProceduralTextureValidateFalseTest::RunTest(const FString& Parameters)
{
	const FString Target = TEXT("/Game/__UeremcpTests/Textures/T_WS08_ValidateFalse_Noise");
	UeremcpMaterialTests::DeleteIfExists(Target);

	const FString Request = FString::Printf(TEXT(R"({
		"protocol_version":"1.0",
		"request_id":"mat-tex-validate-false",
		"action":"create_procedural_texture",
		"target":{"asset_path":"%s"},
		"specification":{"generate":"noise","dimensions":[64,64],"seed":3},
		"options":{"save":true,"validate":false}
	})"), *Target);

	const FString Json = UUeremcpMaterialToolset::CreateProceduralTexture(Request);
	FString Status;
	TestTrue(TEXT("response parseable"), UeremcpMaterialTests::ParseStatus(Json, Status));
	TestEqual(TEXT("partially_completed when validate false"), Status, FString(TEXT("partially_completed")));
	TestFalse(TEXT("must not claim created_and_validated"), Json.Contains(TEXT("created_and_validated")));

	UeremcpMaterialTests::DeleteIfExists(Target);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
