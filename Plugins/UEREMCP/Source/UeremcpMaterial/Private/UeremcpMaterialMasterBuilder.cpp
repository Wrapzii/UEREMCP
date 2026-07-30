// UEREMCP — VFX master material builder (WS-08).

#include "UeremcpMaterialMasterBuilder.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Factories/MaterialFactoryNew.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialFeatureGraph.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	static UMaterial* LoadRegisteredMaterialAtPath(const FString& PackagePath)
	{
		return UeremcpMaterialAssetLoad::TryLoadRegisteredMaterial(PackagePath);
	}

	static bool SaveMaterialAtPath(const FString& PackagePath, UMaterial* Material, int32& InOutOps)
	{
		UEditorAssetSubsystem* AssetSubsystem =
			GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
		if (!AssetSubsystem || !Material || PackagePath.IsEmpty())
		{
			return false;
		}

		Material->MarkPackageDirty();
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
		}
		return Material;
	}
}

FUeremcpMaterialMasterBuildResult UeremcpMaterialMasterBuilder::EnsureMasterMaterial(
	const FUeremcpMaterialMasterBuildRequest& Request)
{
	FUeremcpMaterialMasterBuildResult Result;
	Result.MasterPackagePath = Request.MasterPackagePath;

	if (!UeremcpMaterialPaths::IsUnderTestsRoot(Request.MasterPackagePath))
	{
		Result.Error = TEXT("Master materials may only be created under /Game/__UeremcpTests/.");
		return Result;
	}

	if (!GEditor)
	{
		Result.Error = TEXT("GEditor unavailable — create_vfx_material requires the Unreal Editor.");
		return Result;
	}

	if (UMaterial* Existing = LoadRegisteredMaterialAtPath(Request.MasterPackagePath))
	{
		Result.bSuccess = true;
		Result.bCreated = false;
		Result.MasterMaterial = Existing;
		Result.WiredFeatures = Request.Features;
		return Result;
	}

	FString FolderPath;
	FString AssetName;
	if (!UeremcpMaterialPaths::SplitPackagePath(Request.MasterPackagePath, FolderPath, AssetName))
	{
		Result.Error = FString::Printf(TEXT("Invalid master package path '%s'."), *Request.MasterPackagePath);
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
	Result.MasterMaterial = Material;
	FAssetRegistryModule::AssetCreated(Material);

	const FUeremcpFeatureGraphBuildResult GraphResult = UeremcpMaterialFeatureGraph::BuildGraph(
		Material,
		Request.Features,
		Request.bTrailPurpose);
	Result.InternalOperations += GraphResult.InternalOperations;
	Result.WiredFeatures = GraphResult.WiredFeatures;
	Result.SkippedFeatures = GraphResult.SkippedFeatures;
	Result.InterpretationNotes = GraphResult.InterpretationNotes;
	Result.CapabilityNotes = GraphResult.CapabilityNotes;

	if (!GraphResult.bSuccess)
	{
		Result.Error = GraphResult.Error;
		return Result;
	}

	if (!SaveMaterialAtPath(Request.MasterPackagePath, Material, Result.InternalOperations))
	{
		Result.bSuccess = true;
		Result.CapabilityNotes.Add(
			TEXT("master save unverified under automation — in-process graph exists; disk persistence not proven."));
		Result.InterpretationNotes.Add(
			FString::Printf(TEXT("Failed to save master '%s' to disk."), *Request.MasterPackagePath));
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
