// UEREMCP — in-place adapt of existing Niagara systems (Magecraft + sandbox).

#include "UeremcpNiagaraAdapt.h"

#include "UeremcpNiagaraCompileAwait.h"
#include "UeremcpNiagaraEmitterProperties.h"
#include "UeremcpNiagaraMaterialBinding.h"
#include "UeremcpNiagaraMaterialBindingDiagnostics.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraProbeAssets.h"

#include "Misc/PackageName.h"
#include "NiagaraEmitter.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraVariant.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	FString ContextErrorsToString(const FNiagaraExternalEditContext& Context)
	{
		TArray<FString> Lines;
		for (const FText& Error : Context.Errors)
		{
			Lines.Add(Error.ToString());
		}
		return FString::Join(Lines, TEXT("; "));
	}

	UNiagaraSystem* LoadExistingSystem(const FString& AssetPath, FString& OutError)
	{
		const FString PackagePath = UeremcpNiagaraPaths::PackageFolderFromAssetPath(AssetPath);
		const FString AssetName = UeremcpNiagaraPaths::AssetNameFromAssetPath(AssetPath);
		const FSoftObjectPath ObjectPath(
			FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName));
		UObject* Loaded = ObjectPath.TryLoad();
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Loaded);
		if (!System)
		{
			OutError = FString::Printf(
				TEXT("adapt_niagara_effect could not load NiagaraSystem at '%s'."),
				*AssetPath);
			return nullptr;
		}
		return System;
	}

	void CollectEmitterNames(UNiagaraSystem* System, TArray<FString>& OutNames)
	{
		OutNames.Reset();
		if (!System)
		{
			return;
		}
		const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
		for (const FNiagaraEmitterHandle& Handle : Handles)
		{
			OutNames.Add(Handle.GetName().ToString());
		}
	}

	bool TryReadLinearColorFromArray(const TArray<TSharedPtr<FJsonValue>>* Arr, FLinearColor& OutColor)
	{
		if (!Arr || Arr->Num() < 3)
		{
			return false;
		}
		OutColor.R = static_cast<float>((*Arr)[0]->AsNumber());
		OutColor.G = static_cast<float>((*Arr)[1]->AsNumber());
		OutColor.B = static_cast<float>((*Arr)[2]->AsNumber());
		OutColor.A = Arr->Num() > 3 ? static_cast<float>((*Arr)[3]->AsNumber()) : 1.0f;
		return true;
	}

	FNiagaraExt_UserVariable MakeFloatUserVariable(const FName& ParamName, float Value)
	{
		FNiagaraExt_UserVariable Var;
		Var.Name = ParamName;
		Var.Type = FNiagaraTypeDefinition::GetFloatDef();
		FNiagaraFloat FloatValue;
		FloatValue.Value = Value;
		FNiagaraVariant Variant;
		Variant.SetBytesValue(Var.Type, FloatValue);
		Var.DefaultValue.Set(Var.Type, Variant);
		return Var;
	}

	FNiagaraExt_UserVariable MakeColorUserVariable(const FName& ParamName, const FLinearColor& Color)
	{
		FNiagaraExt_UserVariable Var;
		Var.Name = ParamName;
		Var.Type = FNiagaraTypeDefinition::GetColorDef();
		FNiagaraVariant Variant;
		Variant.SetBytesValue(Var.Type, Color);
		Var.DefaultValue.Set(Var.Type, Variant);
		return Var;
	}

	void ApplyParameters(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const TSharedPtr<FJsonObject>& Parameters,
		TArray<FString>& OutAdded,
		int32& InOutOps)
	{
		if (!Parameters.IsValid() || !System)
		{
			return;
		}

		auto AddVar = [&](const FNiagaraExt_UserVariable& Var)
		{
			UNiagaraExternalEditUtilities::AddUserVariable(System, Var, Context);
			++InOutOps;
			OutAdded.Add(Var.Name.ToString());
		};

		const TArray<TSharedPtr<FJsonValue>>* Primary = nullptr;
		if (Parameters->TryGetArrayField(TEXT("primary_color"), Primary))
		{
			FLinearColor Color;
			if (TryReadLinearColorFromArray(Primary, Color))
			{
				AddVar(MakeColorUserVariable(TEXT("User.Color"), Color));
			}
		}
		double Scale = 0.0;
		if (Parameters->TryGetNumberField(TEXT("scale"), Scale))
		{
			AddVar(MakeFloatUserVariable(TEXT("User.Scale"), static_cast<float>(Scale)));
		}
		double Intensity = 0.0;
		if (Parameters->TryGetNumberField(TEXT("intensity"), Intensity))
		{
			AddVar(MakeFloatUserVariable(TEXT("User.Intensity"), static_cast<float>(Intensity)));
		}
		double Density = 0.0;
		if (Parameters->TryGetNumberField(TEXT("density"), Density)
			|| Parameters->TryGetNumberField(TEXT("mist_density"), Density))
		{
			AddVar(MakeFloatUserVariable(TEXT("User.Density"), static_cast<float>(Density)));
		}

		const bool bIncludeAdapt = Parameters->HasField(TEXT("include_adaptation"))
			|| Parameters->HasField(TEXT("dirtiness"))
			|| Parameters->HasField(TEXT("wetness"))
			|| Parameters->HasField(TEXT("scorch"))
			|| Parameters->HasField(TEXT("vegetation"))
			|| Parameters->HasField(TEXT("temperature"))
			|| Parameters->HasField(TEXT("mastery"));
		if (bIncludeAdapt || Parameters->HasField(TEXT("adapt_secondary_color")))
		{
			auto AddFloatField = [&](const TCHAR* JsonKey, const TCHAR* UserName, float DefaultVal)
			{
				double V = DefaultVal;
				Parameters->TryGetNumberField(JsonKey, V);
				AddVar(MakeFloatUserVariable(FName(UserName), static_cast<float>(V)));
			};
			AddFloatField(TEXT("dirtiness"), TEXT("User.Dirtiness"), 0.f);
			AddFloatField(TEXT("wetness"), TEXT("User.Wetness"), 0.f);
			AddFloatField(TEXT("scorch"), TEXT("User.Scorch"), 0.f);
			AddFloatField(TEXT("vegetation"), TEXT("User.Vegetation"), 0.f);
			AddFloatField(TEXT("temperature"), TEXT("User.Temperature"), 0.f);
			AddFloatField(TEXT("mastery"), TEXT("User.Mastery"), 0.f);

			const TArray<TSharedPtr<FJsonValue>>* AdaptSecondary = nullptr;
			FLinearColor AdaptSecondaryColor(0.35f, 0.22f, 0.10f, 1.0f);
			if (Parameters->TryGetArrayField(TEXT("adapt_secondary_color"), AdaptSecondary))
			{
				TryReadLinearColorFromArray(AdaptSecondary, AdaptSecondaryColor);
			}
			AddVar(MakeColorUserVariable(TEXT("User.AdaptSecondaryColor"), AdaptSecondaryColor));
		}
	}
}

bool FUeremcpNiagaraAdapt::ParseSpecification(
	const TSharedPtr<FJsonObject>& Specification,
	FUeremcpNiagaraAdaptSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FUeremcpNiagaraAdaptSpec();
	OutError.Reset();
	if (!Specification.IsValid())
	{
		OutError = TEXT("adapt_niagara_effect requires a specification object.");
		return false;
	}

	const TSharedPtr<FJsonObject>* Params = nullptr;
	if (Specification->TryGetObjectField(TEXT("parameters"), Params) && Params)
	{
		OutSpec.Parameters = *Params;
	}

	FString MatError;
	if (!FUeremcpNiagaraMaterialBinding::ParseMaterialRequests(
		Specification, OutSpec.MaterialRequests, MatError))
	{
		OutError = MatError;
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Emitters = nullptr;
	if (Specification->TryGetArrayField(TEXT("emitters"), Emitters) && Emitters)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Emitters)
		{
			if (Entry.IsValid() && Entry->Type == EJson::Object)
			{
				OutSpec.EmitterPropertyPatches.Add(Entry->AsObject());
			}
		}
	}

	if (!OutSpec.Parameters.IsValid()
		&& OutSpec.MaterialRequests.Num() == 0
		&& OutSpec.EmitterPropertyPatches.Num() == 0)
	{
		OutError = TEXT(
			"adapt_niagara_effect requires specification.parameters and/or "
			"specification.materials and/or specification.emitters[{sim_target|life_cycle}].");
		return false;
	}
	return true;
}

bool FUeremcpNiagaraAdapt::Run(
	const FUeremcpRequest& Request,
	const FUeremcpNiagaraAdaptSpec& Spec,
	FUeremcpNiagaraAdaptResult& OutResult)
{
	OutResult = FUeremcpNiagaraAdaptResult();
	OutResult.AssetPath = Request.TargetAssetPath;

	if (!UeremcpNiagaraPaths::IsAllowedMutatePath(Request.TargetAssetPath))
	{
		OutResult.Error = UeremcpNiagaraPaths::MutateDeniedReason(Request.TargetAssetPath);
		return false;
	}

	if (Request.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.Summary = FString::Printf(
			TEXT("Dry run: would adapt Niagara system %s (params=%s materials=%d emitters=%d)."),
			*Request.TargetAssetPath,
			Spec.Parameters.IsValid() ? TEXT("yes") : TEXT("no"),
			Spec.MaterialRequests.Num(),
			Spec.EmitterPropertyPatches.Num());
		OutResult.ChecksPerformed.Add(TEXT("niagara.adapt_dry_run"));
		return true;
	}

	FString LoadError;
	UNiagaraSystem* System = LoadExistingSystem(Request.TargetAssetPath, LoadError);
	if (!System)
	{
		OutResult.Error = LoadError;
		return false;
	}

	CollectEmitterNames(System, OutResult.EmitterNames);
	FNiagaraExternalEditContext Context(System);

	ApplyParameters(
		System,
		Context,
		Spec.Parameters,
		OutResult.UserVariablesTouched,
		OutResult.InternalOperations);
	if (Context.HasErrors())
	{
		OutResult.Error = ContextErrorsToString(Context);
		return false;
	}
	if (OutResult.UserVariablesTouched.Num() > 0)
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.adapt_user_variables"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.adapt_user_variables"));
	}

	if (Spec.EmitterPropertyPatches.Num() > 0)
	{
		for (const TSharedPtr<FJsonObject>& Patch : Spec.EmitterPropertyPatches)
		{
			if (!Patch.IsValid())
			{
				continue;
			}
			FString EmitterName;
			Patch->TryGetStringField(TEXT("name"), EmitterName);
			if (EmitterName.IsEmpty())
			{
				Patch->TryGetStringField(TEXT("emitter_name"), EmitterName);
			}
			if (EmitterName.IsEmpty())
			{
				OutResult.ChecksSkipped.Add(TEXT("niagara.adapt_emitter_properties_missing_name"));
				continue;
			}
			FUeremcpNiagaraEmitterPropertyPlan Plan;
			FUeremcpNiagaraEmitterProperties::ParseFromJsonObject(Patch, Plan);
			if (!Plan.HasAny())
			{
				continue;
			}
			TArray<FString> Applied;
			TArray<FString> Warnings;
			FUeremcpNiagaraEmitterProperties::ApplyAll(
				System,
				Context,
				EmitterName,
				Plan,
				OutResult.InternalOperations,
				Applied,
				Warnings);
			OutResult.EmitterPropertiesApplied.Append(Applied);
			OutResult.ChecksSkipped.Append(Warnings);
		}
		if (OutResult.EmitterPropertiesApplied.Num() > 0)
		{
			OutResult.ChecksPerformed.Add(TEXT("niagara.adapt_emitter_properties"));
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.adapt_emitter_properties"));
	}

	TMap<FString, FString> ResolvedMaterialPaths;
	TArray<FUeremcpNiagaraInlineMaterialCreate> InlineCreates;
	TArray<FString> Unresolved;
	if (Spec.MaterialRequests.Num() > 0)
	{
		// Direct paths only on Magecraft — refuse inline create_spec outside sandbox.
		for (const FUeremcpNiagaraMaterialRequest& Req : Spec.MaterialRequests)
		{
			if (Req.CreateSpec.IsValid() && UeremcpNiagaraPaths::IsAllowedMagecraftPath(Request.TargetAssetPath))
			{
				OutResult.Error = TEXT(
					"adapt_niagara_effect on Magecraft does not allow materials.*.create_spec "
					"(inline MI create is sandbox-only). Pass existing RuntimeMaterials paths.");
				return false;
			}
		}

		FString ResolveError;
		if (!FUeremcpNiagaraMaterialBinding::ResolveMaterialPaths(
			Request.TargetAssetPath,
			Spec.MaterialRequests,
			Request.bCompile,
			Request.bValidate,
			Request.bSave,
			ResolvedMaterialPaths,
			InlineCreates,
			Unresolved,
			OutResult.InternalOperations,
			ResolveError))
		{
			OutResult.Error = ResolveError;
			return false;
		}

		OutResult.MaterialBindings.ResolvedMaterialPaths = ResolvedMaterialPaths;
		OutResult.MaterialBindings.InlineMaterialCreates = InlineCreates;
		OutResult.MaterialBindings.UnresolvedMaterialBindings = Unresolved;

		if (ResolvedMaterialPaths.Num() > 0)
		{
			const bool bBindingOk = FUeremcpNiagaraMaterialBinding::ApplyRoleMaterialBindings(
				System,
				OutResult.EmitterNames,
				ResolvedMaterialPaths,
				Spec.MaterialRequests,
				Context,
				OutResult.MaterialBindings,
				OutResult.InternalOperations);
			if (!bBindingOk)
			{
				if (FUeremcpNiagaraMaterialBindingDiagnostics::ShouldContinueAfterBindingFailure(
					OutResult.MaterialBindings))
				{
					FUeremcpNiagaraMaterialBindingDiagnostics::AppendOrphanPartialFailureChecksSkipped(
						OutResult.ChecksSkipped);
				}
				else
				{
					OutResult.Error = TEXT("adapt_niagara_effect material binding failed re-read verification.");
					return false;
				}
			}
			else
			{
				OutResult.ChecksPerformed.Add(TEXT("niagara.adapt_material_bindings"));
			}
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.adapt_material_bindings"));
	}

	if (Request.bCompile)
	{
		FNiagaraExt_SystemCompileState CompileState;
		const int32 TimeoutSeconds = Request.TimeoutMs > 0 ? FMath::Max(1, Request.TimeoutMs / 1000) : 120;
		const FUeremcpNiagaraCompileAwaitResult AwaitResult =
			FUeremcpNiagaraCompileAwait::AwaitCompile(System, Context, TimeoutSeconds, CompileState);
		++OutResult.InternalOperations;
		OutResult.ChecksPerformed.Add(TEXT("niagara.adapt_compile"));
		if (!AwaitResult.Error.IsEmpty())
		{
			OutResult.Error = AwaitResult.Error;
			OutResult.bCompiled = false;
			return false;
		}
		const bool bUpToDate =
			FUeremcpNiagaraCompileAwait::IsAggregateCompileUpToDate(CompileState.AggregateStatus);
		OutResult.bCompiled = AwaitResult.bAwaited && !CompileState.bHasErrors && bUpToDate;
		if (CompileState.bHasErrors)
		{
			OutResult.Error = TEXT("adapt_niagara_effect compile finished with errors.");
			return false;
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.adapt_compile"));
	}

	if (Request.bSave)
	{
		System->MarkPackageDirty();
		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			System->GetOutermost()->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(System->GetOutermost(), System, *PackageFilename, SaveArgs);
		OutResult.bSaved = bSaved;
		++OutResult.InternalOperations;
		OutResult.ChecksPerformed.Add(TEXT("niagara.adapt_save"));
		if (!bSaved)
		{
			OutResult.Error = TEXT("adapt_niagara_effect SavePackage failed.");
			return false;
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.adapt_save"));
	}

	OutResult.bSuccess = true;
	OutResult.Summary = FString::Printf(
		TEXT("Adapted %s (%d emitters, %d user vars, materials=%d)."),
		*Request.TargetAssetPath,
		OutResult.EmitterNames.Num(),
		OutResult.UserVariablesTouched.Num(),
		ResolvedMaterialPaths.Num());
	return true;
}
