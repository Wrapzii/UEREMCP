// UEREMCP — goal-level Niagara effect creation (WS-07 / POC B slice).

#include "UeremcpNiagaraCreate.h"

#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraMaterialBinding.h"
#include "UeremcpNiagaraMaterialBindingDiagnostics.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraProbeAssets.h"
#include "UeremcpNiagaraRoleNames.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraVariant.h"

#include "Misc/PackageName.h"
#include "Misc/App.h"
#include "Containers/Ticker.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const TCHAR* GDefaultSystemTemplate = TEXT("/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight");

	FString RoleToEmitterName(const FString& Role)
	{
		return UeremcpNiagaraRoles::RoleToEmitterName(Role);
	}

	FString ResolveEmitterTemplatePath(const FString& Role)
	{
		return UeremcpNiagaraRoles::ResolveEmitterTemplatePath(Role);
	}

	UObject* LoadSoftPath(const FString& SoftPath)
	{
		if (SoftPath.IsEmpty())
		{
			return nullptr;
		}
		UObject* Loaded = FSoftObjectPath(SoftPath).TryLoad();
		if (Loaded)
		{
			return Loaded;
		}
		const FString AssetName = FPackageName::GetLongPackageAssetName(SoftPath);
		if (!AssetName.IsEmpty())
		{
			return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *SoftPath, *AssetName)).TryLoad();
		}
		return nullptr;
	}

	FString ContextErrorsToString(const FNiagaraExternalEditContext& Context)
	{
		TArray<FString> Lines;
		for (const FText& Error : Context.Errors)
		{
			Lines.Add(Error.ToString());
		}
		return FString::Join(Lines, TEXT("; "));
	}

	bool AssetExistsAtPath(const FString& AssetPath)
	{
		return UeremcpNiagaraProbeAssets::AssetExistsAtPath(AssetPath);
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

	void ApplySpecificationParameters(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const TSharedPtr<FJsonObject>& Parameters,
		TArray<FString>& OutAdded,
		int32& InOutOps)
	{
		if (!Parameters.IsValid())
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

		const TArray<TSharedPtr<FJsonValue>>* Secondary = nullptr;
		if (Parameters->TryGetArrayField(TEXT("secondary_color"), Secondary))
		{
			FLinearColor Color;
			if (TryReadLinearColorFromArray(Secondary, Color))
			{
				AddVar(MakeColorUserVariable(TEXT("User.SecondaryColor"), Color));
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
	}

	bool AwaitCompile(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		int32 TimeoutSeconds,
		FNiagaraExt_SystemCompileState& OutState)
	{
		if (!System)
		{
			return false;
		}

		System->RequestCompile(false);

		// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h:449]
		// PollForCompilationComplete(bFlush=true) wraps QueryCompileComplete(false); pair with
		// FTSTicker so unattended automation advances async compile work (NiagaraToolset_System.cpp:495-520).
		// [VERIFIED: Engine/Source/Runtime/Core/Public/Containers/Ticker.h:81]
		// Do not break on Poll's return value — true means one ActiveCompilation finished, not all.
		const double Deadline = FPlatformTime::Seconds() + static_cast<double>(TimeoutSeconds);
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (!System->HasActiveCompilations()
				&& !System->HasOutstandingCompilationRequests(/*bIncludingGPUShaders=*/false))
			{
				break;
			}

			System->PollForCompilationComplete(/*bFlushRequestCompile=*/true);
			FTSTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
			FPlatformProcess::Sleep(0.001f);
		}

		UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);
		return !OutState.bIsCompiling;
	}

	bool SaveSystemPackage(UNiagaraSystem* System, FString& OutError)
	{
		UPackage* Package = System ? System->GetOutermost() : nullptr;
		if (!Package)
		{
			OutError = TEXT("Niagara system has no package.");
			return false;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;

		if (!UPackage::SavePackage(Package, System, *Filename, SaveArgs))
		{
			OutError = FString::Printf(TEXT("SavePackage failed for '%s'."), *Package->GetName());
			return false;
		}
		return true;
	}
}

bool FUeremcpNiagaraCreate::ParseSpecification(
	const FUeremcpRequest& Request,
	FUeremcpNiagaraCreateSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FUeremcpNiagaraCreateSpec();
	OutError.Reset();

	if (!Request.Specification.IsValid())
	{
		OutError = TEXT("create_niagara_effect requires a specification object.");
		return false;
	}

	const TSharedPtr<FJsonObject>& Spec = Request.Specification;
	if (!Spec->HasField(TEXT("effect_type")))
	{
		OutError = TEXT("create_niagara_effect requires specification.effect_type.");
		return false;
	}

	OutSpec.EffectType = Spec->GetStringField(TEXT("effect_type"));
	Spec->TryGetStringField(TEXT("name"), OutSpec.Name);
	Spec->TryGetStringField(TEXT("element"), OutSpec.Element);

	if (OutSpec.Name.IsEmpty() && !Request.TargetAssetPath.IsEmpty())
	{
		OutSpec.Name = UeremcpNiagaraPaths::AssetNameFromAssetPath(Request.TargetAssetPath);
	}

	const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
	if (Spec->TryGetArrayField(TEXT("components"), Components) && Components)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Components)
		{
			FString RoleString;
			if (Entry->TryGetString(RoleString))
			{
				OutSpec.ComponentRoles.Add(RoleString);
				continue;
			}

			const TSharedPtr<FJsonObject>* RoleObj = nullptr;
			if (Entry->TryGetObject(RoleObj) && RoleObj && RoleObj->IsValid())
			{
				FString Role;
				if ((*RoleObj)->TryGetStringField(TEXT("role"), Role))
				{
					bool bEnabled = true;
					(*RoleObj)->TryGetBoolField(TEXT("enabled"), bEnabled);
					if (bEnabled)
					{
						OutSpec.ComponentRoles.Add(Role);
					}
				}
			}
		}
	}

	if (Spec->HasTypedField<EJson::Object>(TEXT("template_system")))
	{
		const TSharedPtr<FJsonObject> TemplateObj = Spec->GetObjectField(TEXT("template_system"));
		TemplateObj->TryGetStringField(TEXT("asset_path"), OutSpec.TemplateSystemPath);
	}

	if (Spec->HasTypedField<EJson::Object>(TEXT("parameters")))
	{
		OutSpec.Parameters = Spec->GetObjectField(TEXT("parameters"));
	}

	if (!FUeremcpNiagaraMaterialBinding::ParseMaterialRequests(Spec, OutSpec.MaterialRequests, OutError))
	{
		return false;
	}

	return true;
}

bool FUeremcpNiagaraCreate::Run(
	const FUeremcpRequest& Request,
	const FUeremcpNiagaraCreateSpec& Spec,
	FUeremcpNiagaraCreateResult& OutResult)
{
	OutResult = FUeremcpNiagaraCreateResult();

	if (!UeremcpNiagaraPaths::IsAllowedProbePath(Request.TargetAssetPath))
	{
		OutResult.Error = FString::Printf(
			TEXT("create_niagara_effect probes only assets under %s (got '%s')."),
			UeremcpNiagaraPaths::TestsContentRoot,
			*Request.TargetAssetPath);
		return false;
	}

	if (Spec.Name.IsEmpty())
	{
		OutResult.Error = TEXT("create_niagara_effect requires specification.name or target.asset_path.");
		return false;
	}

	const FString PackageFolder = UeremcpNiagaraPaths::PackageFolderFromAssetPath(Request.TargetAssetPath);
	const FString AssetName = Spec.Name;
	const FString CreatedPath = FString::Printf(TEXT("%s/%s"), *PackageFolder, *AssetName);
	const bool bReplaceMode = UeremcpNiagaraProbeAssets::IsReplaceMode(Request.Mode);
	const bool bAssetExists = AssetExistsAtPath(CreatedPath);

	TMap<FString, FString> ResolvedMaterialPaths;
	TArray<FUeremcpNiagaraInlineMaterialCreate> InlineMaterialCreates;
	TArray<FString> UnresolvedMaterialPaths;
	if (Spec.MaterialRequests.Num() > 0 && !Request.bDryRun)
	{
		FString MaterialError;
		if (!FUeremcpNiagaraMaterialBinding::ResolveMaterialPaths(
			AssetName,
			Spec.MaterialRequests,
			Request.bCompile,
			Request.bValidate,
			Request.bSave,
			ResolvedMaterialPaths,
			InlineMaterialCreates,
			UnresolvedMaterialPaths,
			OutResult.InternalOperations,
			MaterialError))
		{
			OutResult.Error = MaterialError;
			return false;
		}
	}

	if (Request.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.CreatedAssetPath = CreatedPath;
		if (bReplaceMode)
		{
			if (bAssetExists)
			{
				OutResult.Summary = FString::Printf(
					TEXT("Dry run: would replace existing probe asset '%s' (effect_type=%s) by deleting and recreating with %d emitter role(s). No editor state touched."),
					*CreatedPath,
					*Spec.EffectType,
					Spec.ComponentRoles.Num());
			}
			else
			{
				OutResult.Summary = FString::Printf(
					TEXT("Dry run: replace mode on '%s' (no existing asset; would create from template with %d emitter role(s)). No editor state touched."),
					*CreatedPath,
					Spec.ComponentRoles.Num());
			}
			OutResult.ChecksSkipped.Add(TEXT("niagara.replace_delete_and_create_dry_run"));
		}
		else
		{
			OutResult.Summary = FString::Printf(
				TEXT("Dry run: would create Niagara effect '%s' (effect_type=%s) with %d emitter role(s) from template. No editor state touched."),
				*CreatedPath,
				*Spec.EffectType,
				Spec.ComponentRoles.Num());
			OutResult.ChecksSkipped.Add(TEXT("niagara.create_all_steps_dry_run"));
		}
		if (Spec.MaterialRequests.Num() > 0)
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.material_bindings"));
		}
		return true;
	}

	if (bAssetExists)
	{
		if (bReplaceMode)
		{
			if (!UeremcpNiagaraProbeAssets::DeleteProbeAssetAtPath(CreatedPath, OutResult.Error))
			{
				return false;
			}
			OutResult.bReplacedExisting = true;
			OutResult.ChecksPerformed.Add(TEXT("niagara.replace_delete_probe_asset"));
		}
		else
		{
			OutResult.Error = FString::Printf(
				TEXT("Asset already exists at '%s'. Use envelope mode 'replace' for idempotent probes under %s."),
				*CreatedPath,
				UeremcpNiagaraPaths::TestsContentRoot);
			return false;
		}
	}
	else if (bReplaceMode)
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.replace_no_existing_asset"));
	}

	const FString TemplatePath = Spec.TemplateSystemPath.IsEmpty()
		? FString(GDefaultSystemTemplate)
		: Spec.TemplateSystemPath;

	UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(LoadSoftPath(TemplatePath));
	if (!TemplateSystem)
	{
		OutResult.Error = FString::Printf(TEXT("Could not load template system at '%s'."), *TemplatePath);
		return false;
	}

	FNiagaraExternalEditContext Context;
	UNiagaraSystem* System = UNiagaraExternalEditUtilities::CreateNiagaraSystem(
		AssetName,
		PackageFolder,
		TemplateSystem,
		Context);
	++OutResult.InternalOperations;

	if (!System || Context.HasErrors())
	{
		OutResult.Error = Context.HasErrors()
			? ContextErrorsToString(Context)
			: TEXT("CreateNiagaraSystem returned null.");
		return false;
	}

	OutResult.ChecksPerformed.Add(TEXT("niagara.create_system_from_template"));
	Context = FNiagaraExternalEditContext(System);

	for (const FString& Role : Spec.ComponentRoles)
	{
		const FString EmitterName = RoleToEmitterName(Role);
		const FString EmitterTemplatePath = ResolveEmitterTemplatePath(Role);
		UNiagaraEmitter* EmitterTemplate = Cast<UNiagaraEmitter>(LoadSoftPath(EmitterTemplatePath));
		if (!EmitterTemplate)
		{
			OutResult.Error = FString::Printf(
				TEXT("Could not load emitter template '%s' for role '%s'."),
				*EmitterTemplatePath,
				*Role);
			return false;
		}

		FNiagaraExt_EmitterTopology Topology;
		UNiagaraExternalEditUtilities::AddEmitter(
			EmitterTemplate,
			FName(*EmitterName),
			Topology,
			Context);
		++OutResult.InternalOperations;

		if (Context.HasErrors())
		{
			OutResult.Error = ContextErrorsToString(Context);
			return false;
		}

		OutResult.EmittersAdded.Add(EmitterName);
	}

	OutResult.ChecksPerformed.Add(TEXT("niagara.add_emitters_from_roles"));

	ApplySpecificationParameters(
		System,
		Context,
		Spec.Parameters,
		OutResult.UserVariablesAdded,
		OutResult.InternalOperations);

	if (Context.HasErrors())
	{
		OutResult.Error = ContextErrorsToString(Context);
		return false;
	}

	if (OutResult.UserVariablesAdded.Num() > 0)
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.add_user_variables"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.add_user_variables"));
	}

	if (ResolvedMaterialPaths.Num() > 0 || InlineMaterialCreates.Num() > 0 || UnresolvedMaterialPaths.Num() > 0)
	{
		OutResult.MaterialBindings.ResolvedMaterialPaths = ResolvedMaterialPaths;
		OutResult.MaterialBindings.InlineMaterialCreates = InlineMaterialCreates;
		OutResult.MaterialBindings.UnresolvedMaterialBindings = UnresolvedMaterialPaths;
	}

	if (ResolvedMaterialPaths.Num() > 0)
	{
		const bool bBindingOk = FUeremcpNiagaraMaterialBinding::ApplyRoleMaterialBindings(
			System,
			OutResult.EmittersAdded,
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
				OutResult.bMaterialBindingPartialFailure = true;
				FUeremcpNiagaraMaterialBindingDiagnostics::AppendOrphanPartialFailureChecksSkipped(
					OutResult.ChecksSkipped);
			}
			else
			{
				OutResult.Error = TEXT("Renderer material binding failed re-read verification.");
				return false;
			}
		}
		else if (OutResult.MaterialBindings.bAllRequestedVerified)
		{
			OutResult.ChecksPerformed.Add(TEXT("niagara.material_bindings"));
		}
		else
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.material_bindings"));
		}
	}
	else if (Spec.MaterialRequests.Num() > 0)
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.material_bindings"));
	}

	if (Request.bCompile)
	{
		FNiagaraExt_SystemCompileState CompileState;
		const int32 TimeoutSeconds = Request.TimeoutMs > 0 ? FMath::Max(1, Request.TimeoutMs / 1000) : 120;
		const bool bAwaited = AwaitCompile(System, Context, TimeoutSeconds, CompileState);
		++OutResult.InternalOperations;
		OutResult.ChecksPerformed.Add(TEXT("niagara.compile_await"));

		const bool bUpToDate = CompileState.AggregateStatus == ENiagaraExt_ScriptCompileStatus::UpToDate
			|| CompileState.AggregateStatus == ENiagaraExt_ScriptCompileStatus::UpToDateWithWarnings;

		OutResult.bCompiled = bAwaited && !CompileState.bHasErrors && bUpToDate;

		if (CompileState.bHasErrors)
		{
			OutResult.Error = TEXT("Niagara compile finished with errors.");
			return false;
		}

		if (CompileState.bIsCompiling)
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.compile_complete_within_timeout"));
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.compile_await"));
	}

	if (Request.bSave)
	{
		FString SaveError;
		if (!SaveSystemPackage(System, SaveError))
		{
			OutResult.Error = SaveError;
			return false;
		}
		OutResult.bSaved = true;
		OutResult.ChecksPerformed.Add(TEXT("niagara.save_package"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.save_package"));
	}

	// POC B gaps — structural re-read, PIE smoke (material_bindings handled above when verified).
	OutResult.ChecksSkipped.Add(TEXT("niagara.structural_re_read"));
	OutResult.ChecksSkipped.Add(TEXT("niagara.runtime_smoke_test"));

	OutResult.CreatedAssetPath = CreatedPath;
	OutResult.bSuccess = true;
	if (OutResult.bReplacedExisting)
	{
		OutResult.Summary = FString::Printf(
			TEXT("Replaced Niagara probe effect '%s' (effect_type=%s): %d emitter(s), %d user variable(s), %d verified material binding(s). Status is not *_validated."),
			*CreatedPath,
			*Spec.EffectType,
			OutResult.EmittersAdded.Num(),
			OutResult.UserVariablesAdded.Num(),
			OutResult.MaterialBindings.RendererBindingsVerified.Num());
	}
	else
	{
		OutResult.Summary = FString::Printf(
			TEXT("Created Niagara probe effect '%s' (effect_type=%s): %d emitter(s), %d user variable(s), %d verified material binding(s). Status is not *_validated."),
			*CreatedPath,
			*Spec.EffectType,
			OutResult.EmittersAdded.Num(),
			OutResult.UserVariablesAdded.Num(),
			OutResult.MaterialBindings.RendererBindingsVerified.Num());
	}

	if (InlineMaterialCreates.Num() > 0)
	{
		int32 FailedInline = 0;
		for (const FUeremcpNiagaraInlineMaterialCreate& Inline : InlineMaterialCreates)
		{
			if (!Inline.bSuccess)
			{
				++FailedInline;
			}
		}
		if (FailedInline > 0)
		{
			OutResult.Summary += FString::Printf(
				TEXT(" %d inline create_spec material(s) failed or unverified."),
				FailedInline);
		}
	}

	if (OutResult.bMaterialBindingPartialFailure)
	{
		const TArray<FString> OrphanedRoles =
			FUeremcpNiagaraMaterialBindingDiagnostics::FindOrphanedInlineCreates(OutResult.MaterialBindings);
		OutResult.Summary += FUeremcpNiagaraMaterialBindingDiagnostics::BuildOrphanPartialFailureSummarySuffix(
			OrphanedRoles.Num());
	}

	return true;
}
