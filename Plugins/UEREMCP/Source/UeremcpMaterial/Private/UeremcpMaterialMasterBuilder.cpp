// UEREMCP — VFX master material builder (WS-08).

#include "UeremcpMaterialMasterBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "ObjectTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialFeatureGraph.h"
#include "UeremcpMaterialFeatures.h"
#include "UeremcpMaterialPaths.h"
#include "UObject/UObjectGlobals.h"

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

	static UMaterial* CreateEmptyMaterial(
		const FString& PackagePath,
		UEditorAssetSubsystem* AssetSubsystem,
		FString& OutError,
		int32& InOutOps)
	{
		FString FolderPath;
		FString AssetName;
		if (!UeremcpMaterialPaths::SplitPackagePath(PackagePath, FolderPath, AssetName))
		{
			OutError = FString::Printf(TEXT("Invalid master package path '%s'."), *PackagePath);
			return nullptr;
		}

		UeremcpMaterialAssetLoad::ReleasePackageForCreate(PackagePath, AssetSubsystem);

		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
		if (UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath))
		{
			TArray<UObject*> ObjectsToDelete;
			ObjectsToDelete.Add(ExistingObject);
			ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
		}

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			OutError = TEXT("CreatePackage failed for master material.");
			return nullptr;
		}

		UMaterial* Material = NewObject<UMaterial>(
			Package,
			UMaterial::StaticClass(),
			FName(*AssetName),
			RF_Public | RF_Standalone);
		if (!Material)
		{
			OutError = FString::Printf(TEXT("NewObject<UMaterial> failed for '%s'."), *PackagePath);
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Material);
		Material->GetOutermost()->MarkPackageDirty();
		++InOutOps;
		return Material;
	}

	static void CollectVerifiedWiredFeatures(
		const UeremcpMaterialFeatures::FFeatureGraphVerifyResult& Verify,
		const TArray<FString>& RequestedFeatures,
		TArray<FString>& OutWiredFeatures)
	{
		for (const FString& Feature : RequestedFeatures)
		{
			if (!UeremcpMaterialFeatures::IsImplementedFeature(Feature))
			{
				continue;
			}
			const bool* bWired = Verify.FeatureWired.Find(Feature);
			if (bWired && *bWired)
			{
				OutWiredFeatures.Add(Feature);
			}
		}
	}

	static bool ApplyGraphToMaster(
		UMaterial* Material,
		const FUeremcpMaterialMasterBuildRequest& Request,
		FUeremcpMaterialMasterBuildResult& Result)
	{
		if (!Material)
		{
			Result.Error = TEXT("Null material for graph build.");
			return false;
		}

		const FUeremcpFeatureGraphBuildResult GraphResult = UeremcpMaterialFeatureGraph::BuildGraph(
			Material,
			Request.Features,
			Request.bTrailPurpose);
		Result.InternalOperations += GraphResult.InternalOperations;
		Result.WiredFeatures = GraphResult.WiredFeatures;
		Result.SkippedFeatures = GraphResult.SkippedFeatures;
		Result.InterpretationNotes.Append(GraphResult.InterpretationNotes);
		Result.CapabilityNotes.Append(GraphResult.CapabilityNotes);

		if (!GraphResult.bSuccess)
		{
			Result.Error = GraphResult.Error;
			Result.MasterMaterial = Material;
			return false;
		}

		if (!SaveMaterialAtPath(Request.MasterPackagePath, Material, Result.InternalOperations))
		{
			Result.bSuccess = true;
			Result.MasterMaterial = Material;
			Result.CapabilityNotes.Add(
				TEXT("master save unverified under automation — in-process graph exists; disk persistence not proven."));
			Result.InterpretationNotes.Add(
				FString::Printf(TEXT("Failed to save master '%s' to disk."), *Request.MasterPackagePath));
			return true;
		}

		Result.bSuccess = true;
		Result.MasterMaterial = Material;
		return true;
	}
}

FUeremcpMaterialMasterBuildResult UeremcpMaterialMasterBuilder::EnsureMasterMaterial(
	const FUeremcpMaterialMasterBuildRequest& Request)
{
	FUeremcpMaterialMasterBuildResult Result;
	Result.MasterPackagePath = Request.MasterPackagePath;

	if (!UeremcpMaterialPaths::IsUnderAllowedScratchRoot(Request.MasterPackagePath))
	{
		Result.Error = TEXT("Master materials may only be created under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/.");
		return Result;
	}

	if (!GEditor)
	{
		Result.Error = TEXT("GEditor unavailable — create_vfx_material requires the Unreal Editor.");
		return Result;
	}

	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();

	if (UMaterial* Existing = LoadRegisteredMaterialAtPath(Request.MasterPackagePath))
	{
		UeremcpMaterialFeatures::FFeatureGraphVerifyResult Verify;
		if (UeremcpMaterialFeatures::VerifyFeatureGraph(Existing, Request.Features, Verify))
		{
			Result.bSuccess = true;
			Result.bCreated = false;
			Result.MasterMaterial = Existing;
			CollectVerifiedWiredFeatures(Verify, Request.Features, Result.WiredFeatures);
			Result.InterpretationNotes.Add(
				TEXT("Reused verified master (feature graph satisfies requested features)."));
			return Result;
		}

		Result.InterpretationNotes.Add(
			FString::Printf(
				TEXT("Existing master '%s' failed feature-graph verification; recreating package."),
				*Request.MasterPackagePath));
		UeremcpMaterialAssetLoad::ReleasePackageForCreate(Request.MasterPackagePath, AssetSubsystem);
	}

	FString CreateError;
	UMaterial* Material = CreateEmptyMaterial(Request.MasterPackagePath, AssetSubsystem, CreateError, Result.InternalOperations);
	if (!Material)
	{
		Result.Error = CreateError;
		return Result;
	}
	Result.bCreated = true;
	Result.InterpretationNotes.Add(
		TEXT("Created feature-driven VFX master via MaterialEditingLibrary (MaterialTools-equivalent substrate)."));

	if (ApplyGraphToMaster(Material, Request, Result))
	{
		return Result;
	}

	return Result;
}
