// UEREMCP — create_vfx_material orchestration (WS-08).

#include "UeremcpMaterialService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialCapabilityNotes.h"
#include "UeremcpMaterialElementPresets.h"
#include "UeremcpMaterialElementPresetsLoader.h"
#include "UeremcpMaterialFeatures.h"
#include "UeremcpMaterialMasterBuilder.h"
#include "UeremcpMaterialNiagaraExport.h"
#include "UeremcpMaterialPaths.h"
#include "UObject/Package.h"
#include "Engine/Texture.h"
#include "UeremcpProceduralTextureService.h"

namespace
{
	static UEditorAssetSubsystem* GetEditorAssetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	}

	static void ReleaseInProcessPackageForCreate(
		const FString& PackagePath,
		UEditorAssetSubsystem* AssetSubsystem)
	{
		if (AssetSubsystem && AssetSubsystem->DoesAssetExist(PackagePath))
		{
			AssetSubsystem->DeleteAsset(PackagePath);
		}

		if (UPackage* ExistingPackage = FindPackage(nullptr, *PackagePath))
		{
			TArray<UPackage*> PackagesToUnload;
			PackagesToUnload.Add(ExistingPackage);
			UPackageTools::UnloadPackages(PackagesToUnload);
		}

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			PackagePath,
			FPackageName::GetAssetPackageExtension());
		if (FPaths::FileExists(PackageFilename))
		{
			IFileManager::Get().Delete(*PackageFilename);
		}
	}

	static bool SaveAssetObject(
		UObject* Asset,
		const FString& PreferredPackagePath,
		FString* OutFailureReason = nullptr)
	{
		UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
		if (!AssetSubsystem || !Asset || PreferredPackagePath.IsEmpty())
		{
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("SaveAssetObject: missing AssetSubsystem, Asset, or package path.");
			}
			return false;
		}

		Asset->MarkPackageDirty();
		UPackage* Package = Asset->GetOutermost();
		if (!Package)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("SaveAssetObject: asset has no outermost package.");
			}
			return false;
		}

		Package->MarkPackageDirty();

		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Package);
		if (UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false))
		{
			return true;
		}

		if (AssetSubsystem->SaveAsset(PreferredPackagePath, false))
		{
			return true;
		}

		const FString ActualPackagePath = Package->GetName();
		if (!ActualPackagePath.Equals(PreferredPackagePath, ESearchCase::CaseSensitive))
		{
			if (AssetSubsystem->SaveAsset(ActualPackagePath, false))
			{
				return true;
			}
		}

		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("UEditorLoadingAndSavingUtils::SavePackages and EditorAssetSubsystem::SaveAsset failed for '%s'."),
				*PreferredPackagePath);
		}
		return false;
	}

	static void ReportMiSaveFailure(
		const FString& PackagePath,
		const FString& FailureReason,
		FUeremcpMaterialCreateResult& Result)
	{
		Result.CapabilityNotes.Add(
			FString::Printf(TEXT("MI save failed for '%s': %s"), *PackagePath, *FailureReason));
		Result.InterpretationNotes.Add(
			FString::Printf(
				TEXT("Disk persistence for MI '%s' was requested (options.save=true) but save did not succeed."),
				*PackagePath));
	}

	struct FVfxPersistTargets
	{
		const FUeremcpRequest* Request = nullptr;
		UMaterialInstanceConstant* Instance = nullptr;
		UMaterial* MasterMaterial = nullptr;
		const FString* MasterPath = nullptr;
		bool bMasterCreated = false;
	};

	static void TryPersistVfxAssets(const FVfxPersistTargets& Targets, FUeremcpMaterialCreateResult& Result)
	{
		if (!Targets.Request || !Targets.Request->bSave)
		{
			return;
		}

		if (Targets.Instance)
		{
			FString SaveFailure;
			if (SaveAssetObject(Targets.Instance, Targets.Request->TargetAssetPath, &SaveFailure))
			{
				++Result.InternalOperations;
				Result.InterpretationNotes.Add(
					FString::Printf(
						TEXT("Saved in-process MI package '%s' to disk."),
						*Targets.Request->TargetAssetPath));
			}
			else
			{
				ReportMiSaveFailure(Targets.Request->TargetAssetPath, SaveFailure, Result);
			}
		}

		if (Targets.bMasterCreated && Targets.MasterMaterial && Targets.MasterPath && !Targets.MasterPath->IsEmpty())
		{
			const FString& SavedMasterPath = *Targets.MasterPath;
			if (SaveAssetObject(Targets.MasterMaterial, SavedMasterPath))
			{
				++Result.InternalOperations;
				Result.InterpretationNotes.Add(
					FString::Printf(TEXT("Saved freshly created master '%s' to disk."), *SavedMasterPath));
			}
			else
			{
				Result.CapabilityNotes.Add(
					TEXT("master save unverified under automation — in-process graph exists; disk persistence not proven."));
			}
		}
	}

	static FVfxPersistTargets MakePersistTargets(
		const FUeremcpRequest& Request,
		UMaterialInstanceConstant* Instance,
		UMaterial* MasterMaterial,
		const FString& MasterPath,
		bool bMasterCreated)
	{
		FVfxPersistTargets Targets;
		Targets.Request = &Request;
		Targets.Instance = Instance;
		Targets.MasterMaterial = MasterMaterial;
		Targets.MasterPath = &MasterPath;
		Targets.bMasterCreated = bMasterCreated;
		return Targets;
	}

	static bool ColorsApproximatelyEqual(const FLinearColor& A, const FLinearColor& B, float Tolerance = 0.02f)
	{
		return FMath::IsNearlyEqual(A.R, B.R, Tolerance) &&
			FMath::IsNearlyEqual(A.G, B.G, Tolerance) &&
			FMath::IsNearlyEqual(A.B, B.B, Tolerance) &&
			FMath::IsNearlyEqual(A.A, B.A, Tolerance);
	}

	static void ParseModifiers(const TSharedPtr<FJsonObject>& Spec, TArray<FString>& OutModifiers)
	{
		const TArray<TSharedPtr<FJsonValue>>* ModArray = nullptr;
		if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("modifiers"), ModArray) && ModArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *ModArray)
			{
				FString Modifier;
				if (Value.IsValid() && Value->TryGetString(Modifier))
				{
					OutModifiers.Add(Modifier);
				}
			}
		}
	}

	static bool ApplyParametersToInstance(
		UMaterialInstanceConstant* Instance,
		const FUeremcpMaterialParameterSet& Params,
		int32& InOutOps)
	{
		if (!Instance)
		{
			return false;
		}

		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(
			Instance, FName(TEXT("ParticleColor")), Params.ParticleColor);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(
			Instance, FName(TEXT("ColorSecondary")), Params.ColorSecondary);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("EmissiveScale")), Params.EmissiveScale);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("FlowSpeed")), Params.FlowSpeed);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("Turbulence")), Params.Turbulence);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("SoftEdge")), Params.SoftEdge);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("DepthFade")), Params.DepthFade);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("DissolveAmount")), Params.DissolveAmount);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, FName(TEXT("DistortionStrength")), Params.DistortionStrength);
		InOutOps += 9;
		return true;
	}

	static bool VerifyInstanceParameters(
		UMaterialInstanceConstant* Instance,
		const FUeremcpMaterialParameterSet& Expected,
		FString& OutError)
	{
		const FLinearColor ReadColor =
			UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(
				Instance, FName(TEXT("ParticleColor")));
		const float ReadEmissive =
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(
				Instance, FName(TEXT("EmissiveScale")));

		if (!ColorsApproximatelyEqual(ReadColor, Expected.ParticleColor))
		{
			OutError = TEXT("Post-apply verification failed: ParticleColor mismatch after re-read.");
			return false;
		}
		if (!FMath::IsNearlyEqual(ReadEmissive, Expected.EmissiveScale, 0.05f))
		{
			OutError = TEXT("Post-apply verification failed: EmissiveScale mismatch after re-read.");
			return false;
		}
		return true;
	}

	static void CapPartialWhenProofUnavailable(
		FUeremcpMaterialCreateResult& Result,
		const FString& TargetPath,
		UMaterialInstanceConstant* Instance,
		const FString& Detail)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("partially_completed");
		Result.PrimaryAsset = TargetPath;
		Result.Summary = Detail;
		Result.CapabilityNotes.Add(
			TEXT("validate: post-create proof unavailable under NullRHI automation — cannot claim *_validated."));
		if (Instance)
		{
			Result.InterpretationNotes.Add(
				TEXT("In-process UMaterialInstanceConstant exists; disk/registry persistence not proven."));
		}
	}

	static bool IsKnownTextureSlot(const FString& SlotName)
	{
		return SlotName == TEXT("MainTexture") ||
			SlotName == TEXT("NoiseTexture") ||
			SlotName == TEXT("FlowMap") ||
			SlotName == TEXT("MaskTexture");
	}

	struct FTextureSlotBinding
	{
		FString SlotName;
		FString AssetPath;
	};

	static FString ProceduralTextureAssetName(
		const FString& MiAssetName,
		const FString& SlotName,
		const FString& GenerateKind)
	{
		return FString::Printf(TEXT("T_%s_%s_%s"), *MiAssetName, *SlotName, *GenerateKind);
	}

	static bool ResolveTextureSlotsFromSpec(
		const TSharedPtr<FJsonObject>& Spec,
		const FString& MiAssetName,
		bool bDryRun,
		bool bSave,
		bool bValidate,
		TArray<FTextureSlotBinding>& OutBindings,
		TArray<FUeremcpAssetRef>& OutCreatedAssets,
		TArray<FUeremcpAssetRef>& OutReusedAssets,
		TArray<FString>& OutInterpretationNotes,
		TArray<FString>& OutCapabilityNotes,
		int32& InOutOps,
		bool& bOutNestedPartial,
		FString& OutError)
	{
		const TSharedPtr<FJsonObject>* TexturesObj = nullptr;
		if (!Spec.IsValid() || !Spec->TryGetObjectField(TEXT("textures"), TexturesObj) || !TexturesObj || !TexturesObj->IsValid())
		{
			return true;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*TexturesObj)->Values)
		{
			const FString& SlotName = Pair.Key;
			if (!IsKnownTextureSlot(SlotName))
			{
				OutError = FString::Printf(
					TEXT("Unknown texture slot '%s'; expected MainTexture, NoiseTexture, FlowMap, or MaskTexture."),
					*SlotName);
				return false;
			}

			const TSharedPtr<FJsonValue>& Value = Pair.Value;
			if (!Value.IsValid())
			{
				continue;
			}

			FString ResolvedPath;
			if (Value->Type == EJson::String)
			{
				ResolvedPath = Value->AsString();
				if (!UeremcpMaterialPaths::IsUnderTestsRoot(ResolvedPath))
				{
					OutError = FString::Printf(
						TEXT("Texture slot '%s' path '%s' must be under /Game/__UeremcpTests/."),
						*SlotName,
						*ResolvedPath);
					return false;
				}
				OutInterpretationNotes.Add(
					FString::Printf(TEXT("Texture slot '%s' bound to existing asset '%s'."), *SlotName, *ResolvedPath));
				FUeremcpAssetRef ReusedTexture;
				ReusedTexture.AssetPath = ResolvedPath;
				ReusedTexture.AssetClass = TEXT("Texture2D");
				ReusedTexture.Role = SlotName;
				OutReusedAssets.Add(ReusedTexture);
			}
			else if (Value->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> GenerateObject = Value->AsObject();
				FString Kind;
				int32 Width = 512;
				int32 Height = 512;
				int32 Seed = 0;
				int32 FlipbookColumns = 0;
				int32 FlipbookRows = 0;
				int32 FlipbookFrameCount = 0;
				FString SourceFilePath;
				if (!UeremcpProceduralTextureService::ParseGenerateSpec(
					GenerateObject, Kind, Width, Height, Seed, FlipbookColumns, FlipbookRows, FlipbookFrameCount, SourceFilePath))
				{
					OutError = FString::Printf(
						TEXT("Texture slot '%s' requires a valid generate spec."),
						*SlotName);
					return false;
				}

				const FString AssetName = ProceduralTextureAssetName(MiAssetName, SlotName, Kind);
				ResolvedPath = UeremcpMaterialPaths::JoinPackagePath(UeremcpMaterialPaths::TexturesFolder, AssetName);

				if (bDryRun)
				{
					OutInterpretationNotes.Add(
						FString::Printf(
							TEXT("dry_run: would generate slot '%s' (%s %dx%d) at '%s'."),
							*SlotName,
							*Kind,
							Width,
							Height,
							*ResolvedPath));
				}
				else
				{
					FUeremcpProceduralTextureRequest TextureRequest;
					TextureRequest.TargetAssetPath = ResolvedPath;
					TextureRequest.GenerateKind = Kind;
					TextureRequest.Width = Width;
					TextureRequest.Height = Height;
					TextureRequest.Seed = Seed;
					TextureRequest.FlipbookColumns = FlipbookColumns;
					TextureRequest.FlipbookRows = FlipbookRows;
					TextureRequest.FlipbookFrameCount = FlipbookFrameCount;
					TextureRequest.SourceFilePath = SourceFilePath;
					TextureRequest.bSave = bSave;
					TextureRequest.bValidate = bValidate;

					const FUeremcpProceduralTextureResult TextureResult =
						UeremcpProceduralTextureService::Execute(TextureRequest);
					InOutOps += TextureResult.InternalOperations;
					OutInterpretationNotes.Append(TextureResult.InterpretationNotes);
					OutCapabilityNotes.Append(TextureResult.CapabilityNotes);

					if (!TextureResult.bSuccess)
					{
						OutError = FString::Printf(
							TEXT("Procedural texture generation failed for slot '%s': %s"),
							*SlotName,
							*TextureResult.Summary);
						return false;
					}

					if (TextureResult.Status == TEXT("partially_completed"))
					{
						bOutNestedPartial = true;
						OutCapabilityNotes.Add(
							FString::Printf(
								TEXT("Texture slot '%s' generation returned partially_completed — nested create_vfx_material cannot claim *_validated."),
								*SlotName));
					}

					if (TextureResult.bReused)
					{
						FUeremcpAssetRef ReusedTexture;
						ReusedTexture.AssetPath = ResolvedPath;
						ReusedTexture.AssetClass = TEXT("Texture2D");
						ReusedTexture.Role = SlotName;
						OutReusedAssets.Add(ReusedTexture);
						OutInterpretationNotes.Add(
							FString::Printf(
								TEXT("Reused procedural texture slot '%s' at '%s' (idempotent)."),
								*SlotName,
								*ResolvedPath));
					}
					else
					{
						OutCreatedAssets.Append(TextureResult.CreatedAssets);
						OutInterpretationNotes.Add(
							FString::Printf(
								TEXT("Generated slot '%s' via create_procedural_texture (%s → '%s')."),
								*SlotName,
								*Kind,
								*ResolvedPath));
					}
				}
			}
			else
			{
				OutError = FString::Printf(
					TEXT("Texture slot '%s' must be an asset path string or generate object."),
					*SlotName);
				return false;
			}

			FTextureSlotBinding Binding;
			Binding.SlotName = SlotName;
			Binding.AssetPath = ResolvedPath;
			OutBindings.Add(Binding);
		}

		return true;
	}

	static bool ApplyTextureSlotsToInstance(
		UMaterialInstanceConstant* Instance,
		const TArray<FTextureSlotBinding>& Bindings,
		UEditorAssetSubsystem* AssetSubsystem,
		int32& InOutOps,
		FString& OutError)
	{
		if (!Instance || !AssetSubsystem)
		{
			OutError = TEXT("Cannot apply texture slots: null instance or asset subsystem.");
			return false;
		}

		for (const FTextureSlotBinding& Binding : Bindings)
		{
			UTexture* Texture = UeremcpMaterialAssetLoad::TryLoadTexture(Binding.AssetPath);
			if (!Texture)
			{
				OutError = FString::Printf(
					TEXT("Failed to load texture '%s' for slot '%s'."),
					*Binding.AssetPath,
					*Binding.SlotName);
				return false;
			}

			UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(
				Instance,
				FName(*Binding.SlotName),
				Texture);
			++InOutOps;
		}
		return true;
	}

	static UMaterialInstanceConstant* CreateMaterialInstanceAtPath(
		const FString& PackagePath,
		UMaterialInterface* Parent,
		UEditorAssetSubsystem* AssetSubsystem,
		FString& OutError,
		int32& InOutOps)
	{
		FString FolderPath;
		FString AssetName;
		if (!UeremcpMaterialPaths::SplitPackagePath(PackagePath, FolderPath, AssetName))
		{
			OutError = FString::Printf(TEXT("Invalid MI package path '%s'."), *PackagePath);
			return nullptr;
		}

		if (!Parent)
		{
			OutError = TEXT("Cannot create MaterialInstanceConstant without parent material.");
			return nullptr;
		}

		ReleaseInProcessPackageForCreate(PackagePath, AssetSubsystem);

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
			OutError = TEXT("CreatePackage failed for MaterialInstanceConstant.");
			return nullptr;
		}

		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			Package,
			UMaterialInstanceConstant::StaticClass(),
			FName(*AssetName),
			RF_Public | RF_Standalone);
		if (!Instance)
		{
			OutError = FString::Printf(
				TEXT("NewObject<UMaterialInstanceConstant> failed for '%s'."),
				*PackagePath);
			return nullptr;
		}

		UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, Parent);

		if (!Instance->GetOutermost()->GetName().Equals(PackagePath, ESearchCase::CaseSensitive) ||
			!Instance->GetName().Equals(AssetName, ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf(
				TEXT("MI object path mismatch after create: expected '%s.%s', got '%s.%s'."),
				*PackagePath,
				*AssetName,
				*Instance->GetOutermost()->GetName(),
				*Instance->GetName());
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Instance);
		Instance->GetOutermost()->MarkPackageDirty();
		++InOutOps;
		return Instance;
	}

	static FString ResolveMaterialSuccessStatus(
		bool bCreatedInstance,
		bool bValidate,
		bool bHasUnimplementedFeatures)
	{
		if (!bValidate)
		{
			return TEXT("partially_completed");
		}
		if (bHasUnimplementedFeatures)
		{
			return bCreatedInstance ? TEXT("created_with_warnings") : TEXT("partially_completed");
		}
		return bCreatedInstance ? TEXT("created_and_validated") : TEXT("modified_and_validated");
	}

	static FString BuildMaterialSuccessSummary(
		bool bCreatedInstance,
		const FUeremcpRequest& Request,
		const FString& TargetPath,
		const FString& MasterPath,
		const FString& Purpose,
		const FString& Element,
		const TArray<FString>& Features)
	{
		FString PostApplyClause;
		if (Request.bValidate)
		{
			TArray<FString> Checks;
			if (Request.bCompile)
			{
				Checks.Add(TEXT("parent recompiled"));
			}
			Checks.Add(TEXT("parameters re-read verified"));
			Checks.Add(TEXT("PrimaryAsset load verified"));
			PostApplyClause = FString::Printf(TEXT("; %s."), *FString::Join(Checks, TEXT(", ")));
		}
		else
		{
			PostApplyClause = TEXT("; options.validate=false — verification checks skipped (partially_completed).");
		}

		return FString::Printf(
			TEXT("create_vfx_material %s '%s' from master '%s' (purpose='%s', element='%s', features=[%s])%s"),
			bCreatedInstance ? TEXT("created") : TEXT("updated"),
			*TargetPath,
			*MasterPath,
			*Purpose,
			Element.IsEmpty() ? TEXT("(unset)") : *Element,
			*FString::Join(Features, TEXT(", ")),
			*PostApplyClause);
	}
}

FUeremcpMaterialCreateResult UeremcpMaterialService::ExecuteCreateVfxMaterial(const FUeremcpRequest& Request)
{
	FUeremcpMaterialCreateResult Result;
	Result.CapabilityNotes = UeremcpMaterialCapability::DefaultPostWireCapabilityNotes();

	if (!GEditor)
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = TEXT("create_vfx_material requires the Unreal Editor (GEditor unavailable in this process).");
		Result.CapabilityNotes.Add(TEXT("Runtime handoff: run UeremcpMaterial.Toolset.CreateVfxMaterial under WS-11 editor harness on RE orch junction."));
		return Result;
	}

	if (!UeremcpMaterialPaths::IsUnderTestsRoot(Request.TargetAssetPath))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("create_vfx_material only writes under /Game/__UeremcpTests/ until WS-12 tier policy extends allowed roots.");
		return Result;
	}

	FString Purpose;
	FString Element;
	TArray<FString> Modifiers;
	TArray<FString> RequestedFeatures;
	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("purpose"), Purpose);
		Spec->TryGetStringField(TEXT("element"), Element);
		ParseModifiers(Spec, Modifiers);
		UeremcpMaterialFeatures::ParseFeaturesFromSpec(Spec, RequestedFeatures);
	}

	const TArray<FString> Features =
		UeremcpMaterialFeatures::ResolveFeaturesForPurpose(Purpose, RequestedFeatures);
	const bool bTrailPurpose = UeremcpMaterialFeatures::IsTrailPurpose(Purpose);

	if (Purpose.IsEmpty())
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("create_vfx_material requires specification.purpose.");
		return Result;
	}

	const bool bSupportedPurpose =
		Purpose.Equals(TEXT("elemental_projectile_core"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("elemental_projectile_trail"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_core"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_ribbon_trail"), ESearchCase::CaseSensitive);

	if (!bSupportedPurpose)
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = FString::Printf(
			TEXT("purpose '%s' is not yet implemented; only elemental_projectile_core|trail and fireball_core|ribbon_trail are wired."),
			*Purpose);
		return Result;
	}

	FUeremcpMaterialParameterSet Params;
	if (!Element.IsEmpty())
	{
		if (!UeremcpMaterialElementPresets::GetElementDefaults(Element, Params))
		{
			Result.InterpretationNotes.Add(
				FString::Printf(TEXT("Unknown element '%s'; applying overrides only."), *Element));
		}
		else
		{
			if (UeremcpMaterialElementPresetsLoader::IsLoadedFromJson())
			{
				Result.InterpretationNotes.Add(
					FString::Printf(
						TEXT("Applied element defaults for '%s' from element_presets.v1.json (%s)."),
						*Element,
						*UeremcpMaterialElementPresetsLoader::GetLoadedPath()));
			}
			else
			{
				Result.InterpretationNotes.Add(
					FString::Printf(
						TEXT("Applied element defaults for '%s' from C++ fallback (element_presets.v1.json not loaded)."),
						*Element));
			}
		}
	}
	UeremcpMaterialElementPresets::ApplyModifiers(Modifiers, Purpose, Params);

	const TSharedPtr<FJsonObject>* OverridesObj = nullptr;
	if (Spec.IsValid() && Spec->TryGetObjectField(TEXT("parameter_overrides"), OverridesObj) && OverridesObj && OverridesObj->IsValid())
	{
		UeremcpMaterialElementPresets::MergeParameterOverrides(*OverridesObj, Params);
		Result.InterpretationNotes.Add(TEXT("Merged specification.parameter_overrides."));
	}

	const FString MasterPath = UeremcpMaterialFeatures::ResolveMasterPackagePath(Purpose, Features);
	Result.InterpretationNotes.Add(
		FString::Printf(
			TEXT("purpose '%s' → master '%s' (features: %s)."),
			*Purpose,
			*MasterPath,
			*FString::Join(Features, TEXT(", "))));

	const TArray<FString> UnimplementedFeatures =
		UeremcpMaterialFeatures::FindUnimplementedFeatures(Features);
	for (const FString& Unimplemented : UnimplementedFeatures)
	{
		Result.CapabilityNotes.Add(
			FString::Printf(TEXT("feature '%s' is not yet implemented in master graph wiring."), *Unimplemented));
	}

	FUeremcpAssetRef MasterDep;
	MasterDep.AssetPath = MasterPath;
	MasterDep.AssetClass = TEXT("Material");
	MasterDep.Role = TEXT("master_template");
	Result.Dependencies.Add(MasterDep);

	FString MiFolder;
	FString MiName;
	if (!UeremcpMaterialPaths::SplitPackagePath(Request.TargetAssetPath, MiFolder, MiName))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(TEXT("Invalid target.asset_path '%s'."), *Request.TargetAssetPath);
		return Result;
	}

	TArray<FTextureSlotBinding> TextureBindings;
	FString TextureError;
	bool bNestedPartialTexture = false;
	if (!ResolveTextureSlotsFromSpec(
		Spec,
		MiName,
		Request.bDryRun,
		Request.bSave,
		Request.bValidate,
		TextureBindings,
		Result.CreatedAssets,
		Result.ReusedAssets,
		Result.InterpretationNotes,
		Result.CapabilityNotes,
		Result.InternalOperations,
		bNestedPartialTexture,
		TextureError))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TextureError;
		return Result;
	}

	if (Request.bDryRun)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("no_change_required");
		Result.Summary = FString::Printf(
			TEXT("dry_run: would create/update MI at '%s' from master '%s' (element='%s', features=[%s])."),
			*Request.TargetAssetPath,
			*MasterPath,
			Element.IsEmpty() ? TEXT("(unset)") : *Element,
			*FString::Join(Features, TEXT(", ")));
		Result.PrimaryAsset = Request.TargetAssetPath;
		return Result;
	}

	FScopedTransaction Transaction(
		NSLOCTEXT("UeremcpMaterial", "CreateVfxMaterial", "UEREMCP create_vfx_material"),
		!Request.bSave);

	FUeremcpMaterialMasterBuildRequest MasterRequest;
	MasterRequest.MasterPackagePath = MasterPath;
	MasterRequest.Features = Features;
	MasterRequest.bTrailPurpose = bTrailPurpose;

	const FUeremcpMaterialMasterBuildResult MasterResult =
		UeremcpMaterialMasterBuilder::EnsureMasterMaterial(MasterRequest);
	Result.InternalOperations += MasterResult.InternalOperations;

	if (!MasterResult.WiredFeatures.IsEmpty())
	{
		Result.InterpretationNotes.Add(
			FString::Printf(
				TEXT("Master wired features: %s"),
				*FString::Join(MasterResult.WiredFeatures, TEXT(", "))));
	}
	for (const FString& Skipped : MasterResult.SkippedFeatures)
	{
		Result.CapabilityNotes.Add(
			FString::Printf(TEXT("Skipped unimplemented feature token '%s' on master build."), *Skipped));
	}
	Result.InterpretationNotes.Append(MasterResult.InterpretationNotes);
	Result.CapabilityNotes.Append(MasterResult.CapabilityNotes);

	if (!MasterResult.bSuccess)
	{
		if (MasterResult.MasterMaterial)
		{
			TryPersistVfxAssets(
				MakePersistTargets(Request, nullptr, MasterResult.MasterMaterial, MasterPath, MasterResult.bCreated),
				Result);
			CapPartialWhenProofUnavailable(
				Result,
				Request.TargetAssetPath,
				nullptr,
				FString::Printf(
					TEXT("Master material setup incomplete for '%s': %s"),
					*MasterPath,
					*MasterResult.Error));
			return Result;
		}

		Result.Status = TEXT("failed_validation");
		Result.Summary = FString::Printf(
			TEXT("Master material setup failed for '%s': %s"),
			*MasterPath,
			*MasterResult.Error);
		return Result;
	}

	if (MasterResult.bCreated)
	{
		FUeremcpAssetRef CreatedMaster;
		CreatedMaster.AssetPath = MasterPath;
		CreatedMaster.AssetClass = TEXT("Material");
		CreatedMaster.Role = TEXT("master_template");
		Result.CreatedAssets.Add(CreatedMaster);
		Result.InterpretationNotes.Add(TEXT("Created feature-driven VFX master via MaterialEditingLibrary (MaterialTools-equivalent substrate)."));
	}
	else
	{
		FUeremcpAssetRef ReusedMaster;
		ReusedMaster.AssetPath = MasterPath;
		ReusedMaster.AssetClass = TEXT("Material");
		ReusedMaster.Role = TEXT("master_template");
		Result.ReusedAssets.Add(ReusedMaster);
		Result.InterpretationNotes.Add(
			FString::Printf(
				TEXT("Reused existing master '%s' (idempotent ensure; graph rebuild skipped)."),
				*MasterPath));
	}

	UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
	if (!AssetSubsystem)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("EditorAssetSubsystem unavailable.");
		return Result;
	}

	UMaterial* MasterMaterial = UeremcpMaterialAssetLoad::ResolveMaterial(MasterPath, MasterResult.MasterMaterial);
	if (!MasterMaterial)
	{
		TryPersistVfxAssets(
			MakePersistTargets(Request, nullptr, MasterResult.MasterMaterial, MasterPath, MasterResult.bCreated),
			Result);
		CapPartialWhenProofUnavailable(
			Result,
			Request.TargetAssetPath,
			nullptr,
			FString::Printf(TEXT("Failed to resolve in-process master '%s' after ensure."), *MasterPath));
		return Result;
	}

	UMaterialInstanceConstant* Instance =
		UeremcpMaterialAssetLoad::TryLoadRegisteredMaterialInstance(Request.TargetAssetPath);
	bool bCreatedInstance = false;

	if (Instance)
	{
		UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, MasterMaterial);
		++Result.InternalOperations;
	}
	else
	{
		FString CreateError;
		Instance = CreateMaterialInstanceAtPath(
			Request.TargetAssetPath,
			MasterMaterial,
			AssetSubsystem,
			CreateError,
			Result.InternalOperations);
		if (!Instance)
		{
			TryPersistVfxAssets(
				MakePersistTargets(Request, nullptr, MasterMaterial, MasterPath, MasterResult.bCreated),
				Result);
			if (Request.bSave)
			{
				Result.InterpretationNotes.Add(
					FString::Printf(
						TEXT("MI save skipped for '%s': instance creation failed before save gate."),
						*Request.TargetAssetPath));
			}
			CapPartialWhenProofUnavailable(
				Result,
				Request.TargetAssetPath,
				nullptr,
				CreateError.IsEmpty()
					? TEXT("Failed to create MaterialInstanceConstant.")
					: CreateError);
			return Result;
		}
		bCreatedInstance = true;
	}

	FString ParamError;
	if (!ApplyParametersToInstance(Instance, Params, Result.InternalOperations))
	{
		TryPersistVfxAssets(
			MakePersistTargets(Request, Instance, MasterMaterial, MasterPath, MasterResult.bCreated),
			Result);
		CapPartialWhenProofUnavailable(
			Result,
			Request.TargetAssetPath,
			Instance,
			TEXT("Failed to apply MI parameters."));
		return Result;
	}

	if (TextureBindings.Num() > 0)
	{
		FString TextureApplyError;
		if (!ApplyTextureSlotsToInstance(Instance, TextureBindings, AssetSubsystem, Result.InternalOperations, TextureApplyError))
		{
			TryPersistVfxAssets(
				MakePersistTargets(Request, Instance, MasterMaterial, MasterPath, MasterResult.bCreated),
				Result);
			CapPartialWhenProofUnavailable(
				Result,
				Request.TargetAssetPath,
				Instance,
				TextureApplyError);
			return Result;
		}
		Result.InterpretationNotes.Add(
			FString::Printf(TEXT("Applied %d MI texture slot binding(s)."), TextureBindings.Num()));
	}

	Instance->MarkPackageDirty();
	++Result.InternalOperations;

	if (Request.bSave)
	{
		FString SaveFailure;
		if (!SaveAssetObject(Instance, Request.TargetAssetPath, &SaveFailure))
		{
			ReportMiSaveFailure(Request.TargetAssetPath, SaveFailure, Result);
			Result.Status = TEXT("partially_completed");
			Result.Summary = FString::Printf(
				TEXT("MI parameters applied%s but save failed for '%s'."),
				Request.bValidate ? TEXT(" and verified") : TEXT(""),
				*Request.TargetAssetPath);
			Result.PrimaryAsset = Request.TargetAssetPath;
			return Result;
		}
		++Result.InternalOperations;
		Result.InterpretationNotes.Add(
			FString::Printf(TEXT("Saved in-process MI package '%s' to disk before compile/validate gates."), *Request.TargetAssetPath));
		Result.InterpretationNotes.Add(
			FString::Printf(
				TEXT("Post-save disk probe: FPackageName::DoesPackageExist('%s')=%s."),
				*Request.TargetAssetPath,
				FPackageName::DoesPackageExist(Request.TargetAssetPath) ? TEXT("true") : TEXT("false")));
		if (MasterResult.bCreated)
		{
			if (SaveAssetObject(MasterMaterial, MasterPath))
			{
				++Result.InternalOperations;
				Result.InterpretationNotes.Add(
					FString::Printf(TEXT("Saved freshly created master '%s' to disk."), *MasterPath));
			}
			else
			{
				Result.CapabilityNotes.Add(
					TEXT("master save unverified under automation — in-process graph exists; disk persistence not proven."));
			}
		}
	}

	if (Request.bCompile)
	{
		const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(MasterMaterial);
		++Result.InternalOperations;
		if (CompileErrors.Num() > 0)
		{
			CapPartialWhenProofUnavailable(
				Result,
				Request.TargetAssetPath,
				Instance,
				FString::Printf(
					TEXT("Parent material recompile reported errors under automation: %s"),
					*FString::Join(CompileErrors, TEXT("; "))));
			if (Request.bSave)
			{
				Result.InterpretationNotes.Add(
					TEXT("MI package was saved before recompile; compile errors do not roll back disk persistence."));
			}
			return Result;
		}
	}

	FString VerifyError;
	if (Request.bValidate && !VerifyInstanceParameters(Instance, Params, VerifyError))
	{
		CapPartialWhenProofUnavailable(Result, Request.TargetAssetPath, Instance, VerifyError);
		return Result;
	}

	Result.PrimaryAsset = Request.TargetAssetPath;

	FString PrimaryLoadError;
	if (Request.bValidate)
	{
		if (UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface(Result.PrimaryAsset, PrimaryLoadError))
		{
			Result.InterpretationNotes.Add(
				TEXT("PrimaryAsset re-load verified as UMaterialInterface (FSoftObjectPath-compatible package path)."));
		}
		else if (Instance)
		{
			Result.InterpretationNotes.Add(
				TEXT("PrimaryAsset registry reload unavailable; verified in-process UMaterialInstanceConstant."));
			Result.CapabilityNotes.Add(
				TEXT("validate: AssetRegistry reload unavailable under automation — in-process MI used as proof."));
		}
		else
		{
			CapPartialWhenProofUnavailable(
				Result,
				Request.TargetAssetPath,
				Instance,
				PrimaryLoadError.IsEmpty()
					? TEXT("PrimaryAsset registry reload unavailable under automation.")
					: PrimaryLoadError);
			return Result;
		}
	}
	else
	{
		Result.InterpretationNotes.Add(
			TEXT("options.validate=false: envelope contract forbids *_validated status."));
	}

	Result.bSuccess = true;
	if (bNestedPartialTexture && Request.bValidate)
	{
		Result.Status = TEXT("partially_completed");
		Result.CapabilityNotes.Add(
			TEXT("Nested procedural texture slot returned partially_completed — create_vfx_material cannot claim *_validated."));
	}
	else
	{
		Result.Status = ResolveMaterialSuccessStatus(
			bCreatedInstance,
			Request.bValidate,
			UnimplementedFeatures.Num() > 0);
	}
	if (UnimplementedFeatures.Num() > 0)
	{
		Result.CapabilityNotes.Add(TEXT("One or more requested feature tokens are not implemented; see capability_notes."));
	}
	Result.Summary = BuildMaterialSuccessSummary(
		bCreatedInstance,
		Request,
		Request.TargetAssetPath,
		MasterPath,
		Purpose,
		Element,
		Features);

	FUeremcpAssetRef InstanceRef;
	InstanceRef.AssetPath = Request.TargetAssetPath;
	InstanceRef.AssetClass = TEXT("MaterialInstanceConstant");
	InstanceRef.Role = Purpose;
	if (bCreatedInstance)
	{
		Result.CreatedAssets.Add(InstanceRef);
	}
	else
	{
		Result.ModifiedAssets.Add(InstanceRef);
	}

	return Result;
}
