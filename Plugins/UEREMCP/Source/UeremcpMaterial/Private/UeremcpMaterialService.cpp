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
#include "UeremcpMaterialMasterBuilder.h"
#include "UeremcpMaterialPaths.h"

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
		InOutOps += 7;
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
				Instance, FName(TEXT("EmissiveScale"));

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
	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("purpose"), Purpose);
		Spec->TryGetStringField(TEXT("element"), Element);
		ParseModifiers(Spec, Modifiers);
	}

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
			Result.InterpretationNotes.Add(
				FString::Printf(TEXT("Applied element defaults for '%s'."), *Element));
		}
	}
	UeremcpMaterialElementPresets::ApplyModifiers(Modifiers, Purpose, Params);

	const TSharedPtr<FJsonObject>* OverridesObj = nullptr;
	if (Spec.IsValid() && Spec->TryGetObjectField(TEXT("parameter_overrides"), OverridesObj) && OverridesObj && OverridesObj->IsValid())
	{
		UeremcpMaterialElementPresets::MergeParameterOverrides(*OverridesObj, Params);
		Result.InterpretationNotes.Add(TEXT("Merged specification.parameter_overrides."));
	}

	const FString MasterPath = UeremcpMaterialElementPresets::ResolveMasterPackagePath(Purpose);
	Result.InterpretationNotes.Add(
		FString::Printf(TEXT("purpose '%s' → master '%s'."), *Purpose, *MasterPath));

	FUeremcpAssetRef MasterDep;
	MasterDep.AssetPath = MasterPath;
	MasterDep.AssetClass = TEXT("Material");
	MasterDep.Role = TEXT("master_template");
	Result.Dependencies.Add(MasterDep);

	if (Request.bDryRun)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("no_change_required");
		Result.Summary = FString::Printf(
			TEXT("dry_run: would create/update MI at '%s' from master '%s' (element='%s')."),
			*Request.TargetAssetPath,
			*MasterPath,
			Element.IsEmpty() ? TEXT("(unset)") : *Element);
		Result.PrimaryAsset = Request.TargetAssetPath;
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("UeremcpMaterial", "CreateVfxMaterial", "UEREMCP create_vfx_material"));

	const FUeremcpMaterialMasterBuildResult MasterResult =
		UeremcpMaterialMasterBuilder::EnsureMasterMaterial(MasterPath);
	Result.InternalOperations += MasterResult.InternalOperations;

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
		Result.InterpretationNotes.Add(TEXT("Created minimal VFX master via MaterialEditingLibrary (MaterialTools-equivalent substrate)."));
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

	FString MiFolder;
	FString MiName;
	if (!UeremcpMaterialPaths::SplitPackagePath(Request.TargetAssetPath, MiFolder, MiName))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(TEXT("Invalid target.asset_path '%s'."), *Request.TargetAssetPath);
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
				TEXT("MI parameters applied and verified but save failed for '%s'."),
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
	Result.bSuccess = true;
	Result.Status = bCreatedInstance ? TEXT("created_and_validated") : TEXT("modified_and_validated");
	Result.Summary = FString::Printf(
		TEXT("create_vfx_material %s '%s' from master '%s' (purpose='%s', element='%s'); parameters applied, parent recompiled, re-read verified."),
		bCreatedInstance ? TEXT("created") : TEXT("updated"),
		*Request.TargetAssetPath,
		*MasterPath,
		*Purpose,
		Element.IsEmpty() ? TEXT("(unset)") : *Element);

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
