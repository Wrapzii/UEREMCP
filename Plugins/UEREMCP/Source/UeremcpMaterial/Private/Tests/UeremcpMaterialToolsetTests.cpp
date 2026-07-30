// Editor automation tests for UeremcpMaterial toolset (WS-08).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
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
	TestEqual(TEXT("created_and_validated"), Status, FString(TEXT("created_and_validated")));

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	TestNotNull(TEXT("asset subsystem"), Subsystem);
	if (Subsystem)
	{
		TestTrue(TEXT("MI asset exists"), Subsystem->DoesAssetExist(Target));
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
	TestEqual(TEXT("created_and_validated"), Status, FString(TEXT("created_and_validated")));

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		TestTrue(TEXT("trail MI exists"), Subsystem->DoesAssetExist(Target));
		const FString FlowMapPath =
			TEXT("/Game/__UeremcpTests/Textures/T_MI_WS08_ProjectileTrail_Ice_FlowMap_flow_map");
		TestTrue(TEXT("generated FlowMap texture exists"), Subsystem->DoesAssetExist(FlowMapPath));
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
	TestEqual(TEXT("created_and_validated"), Status, FString(TEXT("created_and_validated")));

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		TestTrue(TEXT("texture asset exists"), Subsystem->DoesAssetExist(Target));
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
	TestEqual(TEXT("status"), Result.Status, FString(TEXT("created_and_validated")));
	TestFalse(TEXT("PrimaryAsset set"), Result.PrimaryAsset.IsEmpty());

	FString VerifyError;
	TestTrue(
		TEXT("PrimaryAsset loads as UMaterialInterface"),
		UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface(Result.PrimaryAsset, VerifyError));

	UEditorAssetSubsystem* Subsystem = UeremcpMaterialTests::GetAssetSubsystem();
	if (Subsystem)
	{
		TestTrue(TEXT("MI asset exists"), Subsystem->DoesAssetExist(Result.PrimaryAsset));
	}

	UeremcpMaterialTests::CleanupWs08MaterialScratch();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
