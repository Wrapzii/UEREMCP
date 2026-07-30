// UEREMCP — goal-level Niagara effect creation (WS-07 / POC B slice).

#include "UeremcpNiagaraCreate.h"

#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraPaths.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraVariant.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const TCHAR* GDefaultSystemTemplate = TEXT("/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight");

	FString RoleToEmitterName(const FString& Role)
	{
		FString Out;
		TArray<FString> Parts;
		Role.ParseIntoArray(Parts, TEXT("_"), true);
		for (FString& Part : Parts)
		{
			if (Part.Len() > 0)
			{
				Part[0] = FChar::ToUpper(Part[0]);
			}
			Out += Part;
		}
		return Out.IsEmpty() ? Role : Out;
	}

	FString ResolveEmitterTemplatePath(const FString& Role)
	{
		const FString Key = Role.ToLower();
		static const TMap<FString, FString> RoleTemplates = {
			{ TEXT("core"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal") },
			{ TEXT("flame_shell"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst") },
			{ TEXT("sparks"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
			{ TEXT("smoke"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
			{ TEXT("ribbon_trail"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon") },
			{ TEXT("impact_burst"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
		};
		if (const FString* Found = RoleTemplates.Find(Key))
		{
			return *Found;
		}
		return RoleTemplates.FindRef(TEXT("sparks"));
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
		FAssetRegistryModule& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const FString PackagePath = UeremcpNiagaraPaths::PackageFolderFromAssetPath(AssetPath);
		const FString AssetName = UeremcpNiagaraPaths::AssetNameFromAssetPath(AssetPath);
		const FSoftObjectPath ObjectPath(FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName));
		return AssetRegistry.Get().GetAssetByObjectPath(ObjectPath).IsValid();
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
		System->RequestCompile(false);

		const double Deadline = FPlatformTime::Seconds() + static_cast<double>(TimeoutSeconds);
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (!System->HasActiveCompilations()
				&& !System->HasOutstandingCompilationRequests(/*bIncludingGPUShaders=*/false))
			{
				break;
			}
			FPlatformProcess::Sleep(0.1f);
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

	if (Request.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.CreatedAssetPath = CreatedPath;
		OutResult.Summary = FString::Printf(
			TEXT("Dry run: would create Niagara effect '%s' (effect_type=%s) with %d emitter role(s) from template. No editor state touched."),
			*CreatedPath,
			*Spec.EffectType,
			Spec.ComponentRoles.Num());
		OutResult.ChecksSkipped.Add(TEXT("niagara.create_all_steps_dry_run"));
		return true;
	}

	if (AssetExistsAtPath(CreatedPath))
	{
		OutResult.Error = FString::Printf(
			TEXT("Asset already exists at '%s'. Use mode replace or choose a new target path."),
			*CreatedPath);
		return false;
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

	// POC B gaps — materials, renderer binding, structural re-read, PIE smoke.
	OutResult.ChecksSkipped.Add(TEXT("niagara.material_bindings"));
	OutResult.ChecksSkipped.Add(TEXT("niagara.structural_re_read"));
	OutResult.ChecksSkipped.Add(TEXT("niagara.runtime_smoke_test"));

	OutResult.CreatedAssetPath = CreatedPath;
	OutResult.bSuccess = true;
	OutResult.Summary = FString::Printf(
		TEXT("Created Niagara probe effect '%s' (effect_type=%s): %d emitter(s), %d user variable(s). Materials and renderer binding not validated — status is not *_validated."),
		*CreatedPath,
		*Spec.EffectType,
		OutResult.EmittersAdded.Num(),
		OutResult.UserVariablesAdded.Num());

	return true;
}
