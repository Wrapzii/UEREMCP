// UEREMCP — create_vfx_material orchestration (WS-08).

#include "UeremcpMaterialService.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialCapabilityNotes.h"
#include "UeremcpMaterialElementPresets.h"
#include "UeremcpMaterialElementPresetsLoader.h"
#include "UeremcpMaterialFeatures.h"
#include "UeremcpMaterialMasterBuilder.h"
#include "UeremcpMaterialNiagaraExport.h"
#include "UeremcpMaterialPaths.h"
#include "Engine/Texture.h"
#include "UeremcpProceduralTextureService.h"

namespace
{
	static UEditorAssetSubsystem* GetEditorAssetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
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
		TArray<FString>& OutInterpretationNotes,
		TArray<FString>& OutCapabilityNotes,
		int32& InOutOps,
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
				if (!UeremcpProceduralTextureService::ParseGenerateSpec(
					GenerateObject, Kind, Width, Height, Seed, FlipbookColumns, FlipbookRows, FlipbookFrameCount))
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

					OutCreatedAssets.Append(TextureResult.CreatedAssets);
					OutInterpretationNotes.Add(
						FString::Printf(
							TEXT("Generated slot '%s' via create_procedural_texture (%s → '%s')."),
							*SlotName,
							*Kind,
							*ResolvedPath));
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
			UTexture* Texture = Cast<UTexture>(AssetSubsystem->LoadAsset(Binding.AssetPath));
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

	static UMaterialInstanceConstant* CreateMaterialInstance(
		const FString& FolderPath,
		const FString& AssetName,
		UMaterialInterface* Parent,
		FString& OutError,
		int32& InOutOps)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Parent;

		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
			AssetName,
			FolderPath,
			UMaterialInstanceConstant::StaticClass(),
			Factory);

		UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(NewAsset);
		if (!Instance)
		{
			OutError = TEXT("AssetTools.CreateAsset did not return UMaterialInstanceConstant.");
			return nullptr;
		}
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
	if (!ResolveTextureSlotsFromSpec(
		Spec,
		MiName,
		Request.bDryRun,
		Request.bSave,
		Request.bValidate,
		TextureBindings,
		Result.CreatedAssets,
		Result.InterpretationNotes,
		Result.CapabilityNotes,
		Result.InternalOperations,
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

	FScopedTransaction Transaction(NSLOCTEXT("UeremcpMaterial", "CreateVfxMaterial", "UEREMCP create_vfx_material"));

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

	if (!MasterResult.bSuccess)
	{
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

	UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
	if (!AssetSubsystem)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("EditorAssetSubsystem unavailable.");
		return Result;
	}

	UMaterial* MasterMaterial = Cast<UMaterial>(AssetSubsystem->LoadAsset(MasterPath));
	if (!MasterMaterial)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = FString::Printf(TEXT("Failed to load master '%s' after ensure."), *MasterPath);
		return Result;
	}

	const bool bExisted = AssetSubsystem->DoesAssetExist(Request.TargetAssetPath);
	UMaterialInstanceConstant* Instance = nullptr;
	bool bCreatedInstance = false;

	if (bExisted)
	{
		Instance = Cast<UMaterialInstanceConstant>(AssetSubsystem->LoadAsset(Request.TargetAssetPath));
		if (!Instance)
		{
			Result.Status = TEXT("failed_validation");
			Result.Summary = FString::Printf(
				TEXT("Target '%s' exists but is not a MaterialInstanceConstant."),
				*Request.TargetAssetPath);
			return Result;
		}
		UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, MasterMaterial);
		++Result.InternalOperations;
	}
	else
	{
		FString CreateError;
		Instance = CreateMaterialInstance(MiFolder, MiName, MasterMaterial, CreateError, Result.InternalOperations);
		if (!Instance)
		{
			Result.Status = TEXT("failed_validation");
			Result.Summary = CreateError;
			return Result;
		}
		bCreatedInstance = true;
	}

	FString ParamError;
	if (!ApplyParametersToInstance(Instance, Params, Result.InternalOperations))
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("Failed to apply MI parameters.");
		return Result;
	}

	if (TextureBindings.Num() > 0)
	{
		FString TextureApplyError;
		if (!ApplyTextureSlotsToInstance(Instance, TextureBindings, AssetSubsystem, Result.InternalOperations, TextureApplyError))
		{
			Result.Status = TEXT("failed_validation");
			Result.Summary = TextureApplyError;
			return Result;
		}
		Result.InterpretationNotes.Add(
			FString::Printf(TEXT("Applied %d MI texture slot binding(s)."), TextureBindings.Num()));
	}

	Instance->MarkPackageDirty();
	++Result.InternalOperations;

	if (Request.bCompile)
	{
		const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(MasterMaterial);
		++Result.InternalOperations;
		if (CompileErrors.Num() > 0)
		{
			Result.Status = TEXT("failed_validation");
			Result.Summary = FString::Printf(
				TEXT("Parent material recompile failed: %s"),
				*FString::Join(CompileErrors, TEXT("; ")));
			return Result;
		}
	}

	FString VerifyError;
	if (Request.bValidate && !VerifyInstanceParameters(Instance, Params, VerifyError))
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = VerifyError;
		return Result;
	}

	if (Request.bSave)
	{
		if (!AssetSubsystem->SaveAsset(Request.TargetAssetPath, false))
		{
			Result.Status = TEXT("partially_completed");
			Result.Summary = FString::Printf(
				TEXT("MI parameters applied%s but save failed for '%s'."),
				Request.bValidate ? TEXT(" and verified") : TEXT(""),
				*Request.TargetAssetPath);
			Result.PrimaryAsset = Request.TargetAssetPath;
			return Result;
		}
		++Result.InternalOperations;
		if (MasterResult.bCreated)
		{
			AssetSubsystem->SaveAsset(MasterPath, false);
			++Result.InternalOperations;
		}
	}

	Result.PrimaryAsset = Request.TargetAssetPath;

	FString PrimaryLoadError;
	if (Request.bValidate &&
		!UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface(Result.PrimaryAsset, PrimaryLoadError))
	{
		Result.bSuccess = false;
		Result.Status = TEXT("failed_validation");
		Result.Summary = PrimaryLoadError;
		return Result;
	}
	if (Request.bValidate)
	{
		Result.InterpretationNotes.Add(
			TEXT("PrimaryAsset re-load verified as UMaterialInterface (FSoftObjectPath-compatible package path)."));
	}
	else
	{
		Result.InterpretationNotes.Add(
			TEXT("options.validate=false: envelope contract forbids *_validated status."));
	}

	Result.bSuccess = true;
	Result.Status = ResolveMaterialSuccessStatus(
		bCreatedInstance,
		Request.bValidate,
		UnimplementedFeatures.Num() > 0);
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
