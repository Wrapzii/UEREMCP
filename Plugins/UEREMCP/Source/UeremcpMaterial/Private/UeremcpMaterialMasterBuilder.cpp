// UEREMCP — VFX master material builder (WS-08).

#include "UeremcpMaterialMasterBuilder.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/MaterialFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	static UEditorAssetSubsystem* GetEditorAssetSubsystem()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		return GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	}

	static UMaterial* LoadMaterialAtPath(const FString& PackagePath)
	{
		UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
		if (!AssetSubsystem)
		{
			return nullptr;
		}
		UObject* Asset = AssetSubsystem->LoadAsset(PackagePath);
		return Cast<UMaterial>(Asset);
	}

	static bool SavePackagePath(const FString& PackagePath, int32& InOutOps)
	{
		UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
		if (!AssetSubsystem)
		{
			return false;
		}
		const bool bSaved = AssetSubsystem->SaveAsset(PackagePath, false);
		if (bSaved)
		{
			++InOutOps;
		}
		return bSaved;
	}

	static UMaterial* CreateEmptyMaterial(const FString& FolderPath, const FString& AssetName, FString& OutError)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
			AssetName,
			FolderPath,
			UMaterial::StaticClass(),
			Factory);

		UMaterial* Material = Cast<UMaterial>(NewAsset);
		if (!Material)
		{
			OutError = TEXT("AssetTools.CreateAsset did not return UMaterial.");
			return nullptr;
		}
		return Material;
	}

	static bool BuildMinimalVfxMasterGraph(UMaterial* Material, FString& OutError, int32& InOutOps)
	{
		if (!Material)
		{
			OutError = TEXT("Null material.");
			return false;
		}

		Material->BlendMode = BLEND_Additive;
		Material->TwoSided = true;
		Material->SetShadingModel(MSM_Unlit);

		UMaterialExpressionVectorParameter* ColorParam = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionVectorParameter::StaticClass(),
				-500,
				0));
		if (!ColorParam)
		{
			OutError = TEXT("Failed to create ParticleColor expression.");
			return false;
		}
		ColorParam->ParameterName = FName(TEXT("ParticleColor"));
		ColorParam->DefaultValue = FLinearColor::White;
		++InOutOps;

		UMaterialExpressionScalarParameter* EmissiveScale = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass(),
				-500,
				120));
		if (!EmissiveScale)
		{
			OutError = TEXT("Failed to create EmissiveScale expression.");
			return false;
		}
		EmissiveScale->ParameterName = FName(TEXT("EmissiveScale"));
		EmissiveScale->DefaultValue = 1.0f;
		++InOutOps;

		UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionMultiply::StaticClass(),
				-200,
				0));
		if (!Multiply)
		{
			OutError = TEXT("Failed to create multiply expression.");
			return false;
		}
		++InOutOps;

		if (!UMaterialEditingLibrary::ConnectMaterialExpressions(ColorParam, TEXT("RGB"), Multiply, TEXT("A")))
		{
			OutError = TEXT("Failed to wire ParticleColor to multiply.");
			return false;
		}
		++InOutOps;

		if (!UMaterialEditingLibrary::ConnectMaterialExpressions(EmissiveScale, TEXT(""), Multiply, TEXT("B")))
		{
			OutError = TEXT("Failed to wire EmissiveScale to multiply.");
			return false;
		}
		++InOutOps;

		if (!UMaterialEditingLibrary::ConnectMaterialProperty(Multiply, TEXT(""), MP_EmissiveColor))
		{
			OutError = TEXT("Failed to connect multiply to MP_EmissiveColor.");
			return false;
		}
		++InOutOps;

		// Expose secondary scalars for trail/core tuning (not wired in minimal graph v1).
		UMaterialExpressionScalarParameter* FlowSpeed = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass(),
				-500,
				240));
		if (FlowSpeed)
		{
			FlowSpeed->ParameterName = FName(TEXT("FlowSpeed"));
			FlowSpeed->DefaultValue = 0.5f;
			++InOutOps;
		}

		UMaterialExpressionScalarParameter* Turbulence = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass(),
				-500,
				360));
		if (Turbulence)
		{
			Turbulence->ParameterName = FName(TEXT("Turbulence"));
			Turbulence->DefaultValue = 0.5f;
			++InOutOps;
		}

		UMaterialExpressionVectorParameter* ColorSecondary = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionVectorParameter::StaticClass(),
				-500,
				480));
		if (ColorSecondary)
		{
			ColorSecondary->ParameterName = FName(TEXT("ColorSecondary"));
			ColorSecondary->DefaultValue = FLinearColor::White;
			++InOutOps;
		}

		UMaterialExpressionScalarParameter* SoftEdge = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass(),
				-500,
				600));
		if (SoftEdge)
		{
			SoftEdge->ParameterName = FName(TEXT("SoftEdge"));
			SoftEdge->DefaultValue = 0.75f;
			++InOutOps;
		}

		UMaterialExpressionScalarParameter* DepthFade = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass(),
				-500,
				720));
		if (DepthFade)
		{
			DepthFade->ParameterName = FName(TEXT("DepthFade"));
			DepthFade->DefaultValue = 100.0f;
			++InOutOps;
		}

		const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(Material);
		++InOutOps;
		if (CompileErrors.Num() > 0)
		{
			OutError = FString::Printf(
				TEXT("Master recompile failed: %s"),
				*FString::Join(CompileErrors, TEXT("; ")));
			return false;
		}

		Material->MarkPackageDirty();
		return true;
	}
}

FUeremcpMaterialMasterBuildResult UeremcpMaterialMasterBuilder::EnsureMasterMaterial(const FString& MasterPackagePath)
{
	FUeremcpMaterialMasterBuildResult Result;
	Result.MasterPackagePath = MasterPackagePath;

	if (!UeremcpMaterialPaths::IsUnderTestsRoot(MasterPackagePath))
	{
		Result.Error = TEXT("Master materials may only be created under /Game/__UeremcpTests/.");
		return Result;
	}

	if (!GEditor)
	{
		Result.Error = TEXT("GEditor unavailable — create_vfx_material requires the Unreal Editor.");
		return Result;
	}

	UMaterial* Existing = LoadMaterialAtPath(MasterPackagePath);
	if (Existing)
	{
		Result.bSuccess = true;
		Result.bCreated = false;
		return Result;
	}

	FString FolderPath;
	FString AssetName;
	if (!UeremcpMaterialPaths::SplitPackagePath(MasterPackagePath, FolderPath, AssetName))
	{
		Result.Error = FString::Printf(TEXT("Invalid master package path '%s'."), *MasterPackagePath);
		return Result;
	}

	FString CreateError;
	UMaterial* Material = CreateEmptyMaterial(FolderPath, AssetName, CreateError);
	if (!Material)
	{
		Result.Error = CreateError;
		return Result;
	}
	Result.bCreated = true;
	Result.InternalOperations += 1;

	FString BuildError;
	if (!BuildMinimalVfxMasterGraph(Material, BuildError, Result.InternalOperations))
	{
		Result.Error = BuildError;
		return Result;
	}

	if (!SavePackagePath(MasterPackagePath, Result.InternalOperations))
	{
		Result.Error = FString::Printf(TEXT("Failed to save master '%s'."), *MasterPackagePath);
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
