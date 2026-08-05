// UEREMCP — goal-level Niagara effect creation (WS-07 / POC B slice).

#include "UeremcpNiagaraCreate.h"

#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraCompileAwait.h"
#include "UeremcpNiagaraEmitterProperties.h"
#include "UeremcpNiagaraMaterialBinding.h"
#include "UeremcpNiagaraMaterialBindingDiagnostics.h"
#include "UeremcpNiagaraModuleResolve.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraProbeAssets.h"
#include "UeremcpNiagaraRoleNames.h"
#include "UeremcpNiagaraStackInputs.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraVariant.h"

#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	class FUeremcpPhaseTimer
	{
	public:
		explicit FUeremcpPhaseTimer(TMap<FString, double>& InOutTimingMs)
			: TimingMs(InOutTimingMs)
			, LastMark(FPlatformTime::Seconds())
		{
		}

		void MarkPhase(const FString& PhaseKey)
		{
			const double Now = FPlatformTime::Seconds();
			TimingMs.Add(PhaseKey, (Now - LastMark) * 1000.0);
			LastMark = Now;
		}

	private:
		TMap<FString, double>& TimingMs;
		double LastMark;
	};
	const TCHAR* GMinimalSystemTemplate = TEXT("/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight");
	const TCHAR* GLoopingSystemTemplate = TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight");

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

		// Mist / fog knobs (IW-005) — aliases map onto User.* for precipitation roles.
		double Density = 0.0;
		if (Parameters->TryGetNumberField(TEXT("density"), Density)
			|| Parameters->TryGetNumberField(TEXT("mist_density"), Density)
			|| Parameters->TryGetNumberField(TEXT("spawn_rate"), Density))
		{
			AddVar(MakeFloatUserVariable(TEXT("User.Density"), static_cast<float>(Density)));
			AddVar(MakeFloatUserVariable(TEXT("User.MistDensity"), static_cast<float>(Density)));
		}

		double Radius = 0.0;
		if (Parameters->TryGetNumberField(TEXT("radius"), Radius)
			|| Parameters->TryGetNumberField(TEXT("mist_radius"), Radius))
		{
			AddVar(MakeFloatUserVariable(TEXT("User.Radius"), static_cast<float>(Radius)));
		}

		const TArray<TSharedPtr<FJsonValue>>* MistColor = nullptr;
		if (Parameters->TryGetArrayField(TEXT("mist_color"), MistColor)
			|| Parameters->TryGetArrayField(TEXT("fog_color"), MistColor)
			|| Parameters->TryGetArrayField(TEXT("color"), MistColor))
		{
			FLinearColor Color;
			if (TryReadLinearColorFromArray(MistColor, Color))
			{
				AddVar(MakeColorUserVariable(TEXT("User.MistColor"), Color));
				// Also seed primary color when agents only pass mist_color/color.
				if (!Parameters->HasField(TEXT("primary_color")))
				{
					AddVar(MakeColorUserVariable(TEXT("User.Color"), Color));
				}
			}
		}
	}

	bool ApplyParticleColor(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const TArray<FString>& EmitterNames,
		const TSharedPtr<FJsonObject>& Parameters,
		int32& InOutOps,
		TArray<FString>& OutChecks,
		FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Primary = nullptr;
		FLinearColor PrimaryColor;
		if (!System
			|| !Parameters.IsValid()
			|| !Parameters->TryGetArrayField(TEXT("primary_color"), Primary)
			|| !TryReadLinearColorFromArray(Primary, PrimaryColor))
		{
			return true;
		}

		for (const FString& EmitterName : EmitterNames)
		{
			FNiagaraExt_SetParameterEntry ColorEntry;
			ColorEntry.Variable.Name = TEXT("Particles.Color");
			ColorEntry.Variable.Type = FNiagaraTypeDefinition::GetColorDef();
			FNiagaraVariant ColorVariant;
			ColorVariant.SetBytesValue(ColorEntry.Variable.Type, PrimaryColor);
			ColorEntry.DefaultValue.Set(ColorEntry.Variable.Type, ColorVariant);

			FNiagaraExt_StackItemReference ParticleSpawnRef(
				System,
				FName(*EmitterName),
				TEXT("ParticleSpawnScript"));
			FNiagaraExt_ModuleTopology AddedModule;
			UNiagaraExternalEditUtilities::AddSetParametersModule(
				ParticleSpawnRef,
				{ ColorEntry },
				AddedModule,
				Context);
			++InOutOps;
			if (Context.HasErrors())
			{
				OutError = ContextErrorsToString(Context);
				return false;
			}
		}
		OutChecks.Add(TEXT("niagara.set_particles_color"));
		return true;
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

	bool PrepareRuntimeScaffold(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		int32& InOutOps,
		TArray<FString>& OutChecks,
		FString& OutError)
	{
		// The generated emitters are stateful, so the fast-path resolver cannot produce
		// runtime state for this system. Execute the normalized SystemState script below.
		// [VERIFIED: NiagaraSystem.cpp:4218-4230]
		FNiagaraExt_SystemData RuntimeSystemData;
		RuntimeSystemData.PropertyValues = TEXT("{\"bAllowSystemStateFastPath\":false}");
		UNiagaraExternalEditUtilities::SetSystemData(System, RuntimeSystemData, Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			OutError = ContextErrorsToString(Context);
			return false;
		}
		OutChecks.Add(TEXT("niagara.disable_system_state_fast_path"));

		// A freshly cloned template can retain SystemState configured as Once with a
		// zero-second duration. Disabling the resolved fast path only makes Niagara execute
		// this graph; it does not rewrite the graph inputs, so the system still completes at
		// age zero before any emitter spawn script runs.
		// [VERIFIED: NiagaraSystemInstance.cpp:3156-3217]
		const FNiagaraExt_StackItemReference SystemStateModuleRef(
			System,
			NAME_None,
			TEXT("SystemUpdateScript"),
			TEXT("SystemState"));

		FNiagaraExt_StackItemReference LoopDurationRef = SystemStateModuleRef;
		LoopDurationRef.InputNameStack.Add(TEXT("Loop Duration"));
		FNiagaraExt_StackInputValue LoopDurationValue;
		FNiagaraFloat& LoopDuration = LoopDurationValue.InitializeAs<FNiagaraFloat>();
		LoopDuration.Value = 5.0f;
		UNiagaraExternalEditUtilities::SetStackInputData(
			LoopDurationRef,
			LoopDurationValue,
			Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			OutError = ContextErrorsToString(Context);
			return false;
		}

		FNiagaraExt_StackItemReference LoopBehaviorRef = SystemStateModuleRef;
		LoopBehaviorRef.InputNameStack.Add(TEXT("Loop Behavior"));
		FNiagaraExt_StackInputValue LoopBehaviorValue;
		TArray<FNiagaraExt_ModuleInputValues> SystemUpdateInputValues;
		UNiagaraExternalEditUtilities::GetScriptStackInputValues(
			FNiagaraExt_StackItemReference(System, NAME_None, TEXT("SystemUpdateScript")),
			SystemUpdateInputValues,
			Context);
		if (Context.HasErrors())
		{
			OutError = ContextErrorsToString(Context);
			return false;
		}
		for (const FNiagaraExt_ModuleInputValues& ModuleValues : SystemUpdateInputValues)
		{
			if (ModuleValues.ModuleName != TEXT("SystemState"))
			{
				continue;
			}
			for (const FNiagaraExt_StackInputValueEntry& Input : ModuleValues.Inputs)
			{
				if (Input.Name == TEXT("Loop Behavior"))
				{
					LoopBehaviorValue = Input.Value;
					break;
				}
			}
		}

		FNiagaraExt_StackInputData_Enum* LoopBehavior =
			LoopBehaviorValue.GetMutablePtr<FNiagaraExt_StackInputData_Enum>();
		if (!LoopBehavior || !LoopBehavior->Enum)
		{
			OutError = TEXT("SystemState Loop Behavior is not an enum input.");
			return false;
		}

		bool bFoundInfinite = false;
		for (int32 EnumIndex = 0; EnumIndex < LoopBehavior->Enum->NumEnums(); ++EnumIndex)
		{
			if (LoopBehavior->Enum->GetDisplayNameTextByIndex(EnumIndex).ToString().Equals(
					TEXT("Infinite"),
					ESearchCase::IgnoreCase))
			{
				LoopBehavior->EnumName = LoopBehavior->Enum->GetNameByIndex(EnumIndex);
				LoopBehavior->DisplayName =
					LoopBehavior->Enum->GetDisplayNameTextByIndex(EnumIndex);
				bFoundInfinite = true;
				break;
			}
		}
		if (!bFoundInfinite)
		{
			OutError = TEXT("SystemState Loop Behavior enum has no Infinite value.");
			return false;
		}

		UNiagaraExternalEditUtilities::SetStackInputData(
			LoopBehaviorRef,
			LoopBehaviorValue,
			Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			OutError = ContextErrorsToString(Context);
			return false;
		}
		OutChecks.Add(TEXT("niagara.normalize_system_lifecycle_infinite"));

		FNiagaraExt_SystemSummary InitialSummary;
		UNiagaraExternalEditUtilities::GetSystemSummary(System, InitialSummary, Context);
		++InOutOps;
		for (const FNiagaraExt_EmitterSummary& InitialEmitter : InitialSummary.Emitters)
		{
			const FNiagaraExt_StackItemReference InitialEmitterRef(System, InitialEmitter.EmitterName);
			UNiagaraExternalEditUtilities::RemoveEmitter(InitialEmitterRef, Context);
			++InOutOps;
			if (Context.HasErrors())
			{
				OutError = ContextErrorsToString(Context);
				return false;
			}
		}
		OutChecks.Add(TEXT("niagara.remove_template_emitters"));
		return true;
	}

	bool ApplyCustomEmitterStack(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FUeremcpNiagaraEmitterPlan& Plan,
		FUeremcpNiagaraCreateResult& OutResult)
	{
		for (const FUeremcpNiagaraModulePlan& Mod : Plan.Modules)
		{
			FString LoadError;
			UNiagaraScript* Script =
				UeremcpNiagaraModuleResolve::LoadModuleScript(Mod.AssetPath, LoadError);
			if (!Script)
			{
				OutResult.Error = FString::Printf(
					TEXT("emitter '%s' module '%s': %s"),
					*Plan.Name,
					*Mod.Name,
					*LoadError);
				return false;
			}

			FNiagaraExt_StackItemReference LocationRef(
				System, FName(*Plan.Name), FName(*Mod.ScriptUsage));
			FNiagaraExt_ModuleTopology Added;
			// [VERIFIED: NiagaraExternalSystemEditorUtilities.h:1367 AddModule]
			UNiagaraExternalEditUtilities::AddModule(LocationRef, Script, Added, Context);
			++OutResult.InternalOperations;
			if (Context.HasErrors())
			{
				OutResult.Error = ContextErrorsToString(Context);
				return false;
			}

			const FName AddedName = Added.ModuleName.IsNone()
				? FName(*Mod.Name)
				: Added.ModuleName;
			const FString Key = FString::Printf(
				TEXT("%s/%s/%s (%s)"),
				*Plan.Name,
				*Mod.ScriptUsage,
				*AddedName.ToString(),
				*Mod.AssetPath);
			OutResult.ModulesAdded.Add(Key);

			if (!Mod.bEnabled)
			{
				FNiagaraExt_StackItemReference ModuleRef(
					System, FName(*Plan.Name), FName(*Mod.ScriptUsage), AddedName);
				UNiagaraExternalEditUtilities::SetModuleEnabled(ModuleRef, false, Context);
				++OutResult.InternalOperations;
				if (Context.HasErrors())
				{
					OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
					Context.Errors.Reset();
				}
			}

			TArray<FString> AppliedInputs;
			FUeremcpNiagaraStackInputs::ApplyModuleInputs(
				System,
				Context,
				Plan.Name,
				Mod.ScriptUsage,
				AddedName,
				Mod.Inputs,
				OutResult.InternalOperations,
				OutResult.LossyWarnings,
				AppliedInputs);
		}

		if (!Plan.RendererType.IsEmpty())
		{
			FString RendererError;
			TSubclassOf<UNiagaraRendererProperties> RendererClass =
				UeremcpNiagaraModuleResolve::ResolveRendererClass(Plan.RendererType, RendererError);
			if (!RendererClass)
			{
				OutResult.LossyWarnings.Add(RendererError.IsEmpty()
					? FString::Printf(TEXT("emitter '%s': unknown renderer"), *Plan.Name)
					: RendererError);
			}
			else
			{
				FNiagaraExt_StackItemReference EmitterRef(System, FName(*Plan.Name));
				FNiagaraExt_RendererRef OutRef;
				// [VERIFIED: NiagaraExternalSystemEditorUtilities.h:1364 AddRenderer]
				UNiagaraExternalEditUtilities::AddRenderer(
					EmitterRef, RendererClass, OutRef, Context);
				++OutResult.InternalOperations;
				if (Context.HasErrors())
				{
					OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
					Context.Errors.Reset();
				}
				else
				{
					OutResult.RenderersAdded.Add(FString::Printf(
						TEXT("%s:%s"), *Plan.Name, *Plan.RendererType));
				}
			}
		}

		FUeremcpNiagaraEmitterPropertyPlan Props;
		Props.SimTarget = Plan.SimTarget;
		Props.bHasEnabled = Plan.bHasEnabled;
		Props.bEnabled = Plan.bEnabled;
		Props.LifeCycleMode = Plan.LifeCycleMode;
		Props.LoopBehavior = Plan.LoopBehavior;
		Props.LoopDuration = Plan.LoopDuration;
		Props.InactiveResponse = Plan.InactiveResponse;
		if (Props.HasAny())
		{
			TArray<FString> AppliedProps;
			FUeremcpNiagaraEmitterProperties::ApplyAll(
				System,
				Context,
				Plan.Name,
				Props,
				OutResult.InternalOperations,
				AppliedProps,
				OutResult.LossyWarnings);
			OutResult.ChecksPerformed.Append(AppliedProps);
			if (AppliedProps.Num() > 0)
			{
				OutResult.ChecksPerformed.Add(TEXT("niagara.emitter_properties_write"));
			}
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
	if (Spec->HasTypedField<EJson::Object>(TEXT("base_system")))
	{
		const TSharedPtr<FJsonObject> BaseObj = Spec->GetObjectField(TEXT("base_system"));
		BaseObj->TryGetStringField(TEXT("asset_path"), OutSpec.BaseSystemPath);
	}

	if (Spec->HasTypedField<EJson::Object>(TEXT("parameters")))
	{
		OutSpec.Parameters = Spec->GetObjectField(TEXT("parameters"));
	}

	if (!FUeremcpNiagaraMaterialBinding::ParseMaterialRequests(Spec, OutSpec.MaterialRequests, OutError))
	{
		return false;
	}

	// specification.emitters[{name, modules[]}] — PRIMARY path for LLM-authored stacks.
	// role / template_path remain optional shortcuts (preset kits), not required.
	const TArray<TSharedPtr<FJsonValue>>* EmittersArr = nullptr;
	if (Spec->TryGetArrayField(TEXT("emitters"), EmittersArr) && EmittersArr)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *EmittersArr)
		{
			FUeremcpNiagaraEmitterPlan Plan;
			if (Entry->TryGetString(Plan.Role))
			{
				// Shorthand: emitters: ["sparks", "circle"] — optional role shortcut.
			}
			else
			{
				const TSharedPtr<FJsonObject>* EmitterObj = nullptr;
				if (!Entry->TryGetObject(EmitterObj) || !EmitterObj || !EmitterObj->IsValid())
				{
					OutError = TEXT("specification.emitters entries must be strings or objects.");
					return false;
				}
				(*EmitterObj)->TryGetStringField(TEXT("name"), Plan.Name);
				(*EmitterObj)->TryGetStringField(TEXT("role"), Plan.Role);
				(*EmitterObj)->TryGetStringField(TEXT("template_path"), Plan.TemplatePath);
				if (Plan.TemplatePath.IsEmpty())
				{
					(*EmitterObj)->TryGetStringField(TEXT("emitter_template"), Plan.TemplatePath);
				}
				if ((*EmitterObj)->HasField(TEXT("enabled")))
				{
					Plan.bEnabled = (*EmitterObj)->GetBoolField(TEXT("enabled"));
					Plan.bHasEnabled = true;
				}

				FUeremcpNiagaraEmitterPropertyPlan PropPlan;
				FUeremcpNiagaraEmitterProperties::ParseFromJsonObject(*EmitterObj, PropPlan);
				Plan.SimTarget = PropPlan.SimTarget;
				Plan.LifeCycleMode = PropPlan.LifeCycleMode;
				Plan.LoopBehavior = PropPlan.LoopBehavior;
				Plan.LoopDuration = PropPlan.LoopDuration;
				Plan.InactiveResponse = PropPlan.InactiveResponse;
				if (PropPlan.bHasEnabled)
				{
					Plan.bEnabled = PropPlan.bEnabled;
					Plan.bHasEnabled = true;
				}

				const TSharedPtr<FJsonObject>* RendererObj = nullptr;
				if ((*EmitterObj)->TryGetObjectField(TEXT("renderer"), RendererObj)
					&& RendererObj && (*RendererObj).IsValid())
				{
					(*RendererObj)->TryGetStringField(TEXT("type"), Plan.RendererType);
					if (Plan.RendererType.IsEmpty())
					{
						(*RendererObj)->TryGetStringField(TEXT("renderer_type"), Plan.RendererType);
					}
				}
				else
				{
					(*EmitterObj)->TryGetStringField(TEXT("renderer"), Plan.RendererType);
					if (Plan.RendererType.IsEmpty())
					{
						(*EmitterObj)->TryGetStringField(TEXT("renderer_type"), Plan.RendererType);
					}
				}

				const TArray<TSharedPtr<FJsonValue>>* ModulesArr = nullptr;
				if ((*EmitterObj)->TryGetArrayField(TEXT("modules"), ModulesArr) && ModulesArr)
				{
					for (const TSharedPtr<FJsonValue>& ModVal : *ModulesArr)
					{
						FUeremcpNiagaraModulePlan Mod;
						if (ModVal->TryGetString(Mod.Name))
						{
							// Shorthand: modules: ["SpawnRate", "spawn_rate"]
							Mod.PrimitiveId = Mod.Name;
						}
						else
						{
							const TSharedPtr<FJsonObject>* ModObj = nullptr;
							if (!ModVal->TryGetObject(ModObj) || !ModObj || !ModObj->IsValid())
							{
								OutError = TEXT(
									"specification.emitters[].modules entries must be strings or objects.");
								return false;
							}
							(*ModObj)->TryGetStringField(TEXT("primitive_id"), Mod.PrimitiveId);
							(*ModObj)->TryGetStringField(TEXT("name"), Mod.Name);
							(*ModObj)->TryGetStringField(TEXT("asset_path"), Mod.AssetPath);
							if (Mod.AssetPath.IsEmpty())
							{
								(*ModObj)->TryGetStringField(TEXT("module_script"), Mod.AssetPath);
							}
							if (Mod.AssetPath.IsEmpty())
							{
								(*ModObj)->TryGetStringField(TEXT("script_path"), Mod.AssetPath);
							}
							(*ModObj)->TryGetStringField(TEXT("script"), Mod.ScriptUsage);
							if (Mod.ScriptUsage.IsEmpty())
							{
								(*ModObj)->TryGetStringField(TEXT("script_usage"), Mod.ScriptUsage);
							}
							if ((*ModObj)->HasField(TEXT("enabled")))
							{
								Mod.bEnabled = (*ModObj)->GetBoolField(TEXT("enabled"));
							}
							const TSharedPtr<FJsonObject>* InputsObj = nullptr;
							if ((*ModObj)->TryGetObjectField(TEXT("inputs"), InputsObj)
								&& InputsObj && (*InputsObj).IsValid())
							{
								Mod.Inputs = *InputsObj;
							}
						}
						if (Mod.PrimitiveId.IsEmpty() && Mod.Name.IsEmpty() && Mod.AssetPath.IsEmpty())
						{
							OutError = TEXT(
								"emitters[].modules[] requires primitive_id or name "
								"and/or asset_path (module_script).");
							return false;
						}
						Plan.Modules.Add(Mod);
					}
				}
			}

			const bool bHasCustomModules = Plan.Modules.Num() > 0;
			if (Plan.Role.IsEmpty() && Plan.TemplatePath.IsEmpty() && !bHasCustomModules
				&& Plan.Name.IsEmpty())
			{
				OutError = TEXT(
					"specification.emitters[] requires name + modules[], and/or role, "
					"and/or template_path.");
				return false;
			}
			if (bHasCustomModules)
			{
				Plan.bCustomModuleStack = true;
			}
			OutSpec.Emitters.Add(Plan);
			if (!Plan.Role.IsEmpty() && !OutSpec.ComponentRoles.Contains(Plan.Role))
			{
				OutSpec.ComponentRoles.Add(Plan.Role);
			}
		}
	}

	if (OutSpec.ComponentRoles.Num() == 0 && OutSpec.Emitters.Num() == 0)
	{
		// Optional shortcut only: default by effect_type when agents omit emitters entirely.
		OutSpec.ComponentRoles =
			UeremcpNiagaraRoles::DefaultComponentRolesForEffectType(OutSpec.EffectType);
	}

	// Resolve template substrate + names. Custom modules[] → Minimal substrate.
	TSet<FString> PlannedNames;
	int32 AnonymousIndex = 0;
	for (FUeremcpNiagaraEmitterPlan& Plan : OutSpec.Emitters)
	{
		if (Plan.Name.IsEmpty())
		{
			if (!Plan.Role.IsEmpty())
			{
				Plan.Name = UeremcpNiagaraRoles::RoleToEmitterName(Plan.Role);
			}
			else
			{
				++AnonymousIndex;
				Plan.Name = FString::Printf(TEXT("CustomEmitter%d"), AnonymousIndex);
			}
		}

		for (FUeremcpNiagaraModulePlan& Mod : Plan.Modules)
		{
			const FString LookupKey = !Mod.PrimitiveId.IsEmpty()
				? Mod.PrimitiveId
				: Mod.Name;
			Mod.ScriptUsage = UeremcpNiagaraModuleResolve::NormalizeScriptUsage(Mod.ScriptUsage);
			if (Mod.ScriptUsage.IsEmpty())
			{
				Mod.ScriptUsage = UeremcpNiagaraModuleResolve::DefaultScriptUsageForModule(
					LookupKey.IsEmpty() ? Mod.AssetPath : LookupKey);
			}
			FString ResolvedPath;
			FString ResolveError;
			if (!UeremcpNiagaraModuleResolve::ResolveModuleAssetPath(
				LookupKey, Mod.AssetPath, ResolvedPath, ResolveError))
			{
				OutError = FString::Printf(
					TEXT("emitter '%s': %s"),
					*Plan.Name,
					*ResolveError);
				return false;
			}
			Mod.AssetPath = ResolvedPath;
			if (Mod.Name.IsEmpty())
			{
				Mod.Name = FPackageName::GetLongPackageAssetName(ResolvedPath);
			}
		}

		if (Plan.TemplatePath.IsEmpty())
		{
			if (Plan.bCustomModuleStack || Plan.Modules.Num() > 0)
			{
				// LLM-defined stacks: clone Minimal then AddModule — not a preset spell kit.
				// [VERIFIED: DefaultNiagara.ini DefaultEmptyEmitter → Minimal]
				Plan.TemplatePath = UeremcpNiagaraModuleResolve::MinimalEmitterTemplatePath();
				Plan.bCustomModuleStack = true;
			}
			else if (!Plan.Role.IsEmpty())
			{
				Plan.TemplatePath = UeremcpNiagaraRoles::ResolveEmitterTemplatePath(Plan.Role);
			}
			else
			{
				OutError = FString::Printf(
					TEXT("emitter '%s' needs modules[], role, or template_path."),
					*Plan.Name);
				return false;
			}
		}
		PlannedNames.Add(Plan.Name);
	}
	for (const FString& Role : OutSpec.ComponentRoles)
	{
		const FString EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Role);
		if (PlannedNames.Contains(EmitterName))
		{
			continue;
		}
		FUeremcpNiagaraEmitterPlan Plan;
		Plan.Role = Role;
		Plan.Name = EmitterName;
		Plan.TemplatePath = UeremcpNiagaraRoles::ResolveEmitterTemplatePath(Role);
		OutSpec.Emitters.Add(Plan);
		PlannedNames.Add(Plan.Name);
	}

	if (OutSpec.Emitters.Num() == 0 && OutSpec.BaseSystemPath.IsEmpty())
	{
		OutError = TEXT(
			"create_niagara_effect requires specification.emitters[{name,modules[]}] "
			"(primary), specification.components / emitters role shortcuts, "
			"or a known effect_type default. Empty shells are rejected.");
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

	if (!UeremcpNiagaraPaths::IsAllowedMutatePath(Request.TargetAssetPath))
	{
		OutResult.Error = FString::Printf(
			TEXT("create_niagara_effect only assets under %s (got '%s')."),
			*UeremcpNiagaraPaths::AllowedMutateRootsDescription(),
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

	FUeremcpPhaseTimer PhaseTimer(OutResult.TimingMs);

	TMap<FString, FString> ResolvedMaterialPaths;
	TArray<FUeremcpNiagaraInlineMaterialCreate> InlineMaterialCreates;
	TArray<FString> UnresolvedMaterialPaths;
	if (Spec.MaterialRequests.Num() > 0 && !Request.bDryRun)
	{
		FString MaterialError;
		if (!FUeremcpNiagaraMaterialBinding::ResolveMaterialPaths(
			CreatedPath,
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
		const int32 EmitterPlanCount = Spec.Emitters.Num() > 0
			? Spec.Emitters.Num()
			: Spec.ComponentRoles.Num();
		int32 ModulePlanCount = 0;
		int32 CustomEmitterCount = 0;
		for (const FUeremcpNiagaraEmitterPlan& Plan : Spec.Emitters)
		{
			ModulePlanCount += Plan.Modules.Num();
			if (Plan.bCustomModuleStack)
			{
				++CustomEmitterCount;
			}
		}
		const FString StructureIntent = Spec.BaseSystemPath.IsEmpty()
			? FString::Printf(
				TEXT("create from template with %d emitter(s), %d module(s) (%d custom Minimal stacks)"),
				EmitterPlanCount,
				ModulePlanCount,
				CustomEmitterCount)
			: FString::Printf(
				TEXT("inherit emitter structure from '%s' and add %d variation emitter(s)"),
				*Spec.BaseSystemPath,
				EmitterPlanCount);
		if (bReplaceMode)
		{
			if (bAssetExists)
			{
				OutResult.Summary = FString::Printf(
					TEXT("Dry run: would replace existing probe asset '%s' (effect_type=%s), %s. No editor state touched."),
					*CreatedPath,
					*Spec.EffectType,
					*StructureIntent);
			}
			else
			{
				OutResult.Summary = FString::Printf(
					TEXT("Dry run: replace mode on '%s' (no existing asset); would %s. No editor state touched."),
					*CreatedPath,
					*StructureIntent);
			}
			OutResult.ChecksSkipped.Add(TEXT("niagara.replace_delete_and_create_dry_run"));
		}
		else
		{
			OutResult.Summary = FString::Printf(
				TEXT("Dry run: would create Niagara effect '%s' (effect_type=%s), %s. No editor state touched."),
				*CreatedPath,
				*Spec.EffectType,
				*StructureIntent);
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
			if (UeremcpNiagaraPaths::IsAllowedMagecraftPath(CreatedPath)
				&& !UeremcpNiagaraPaths::IsAllowedProbePath(CreatedPath))
			{
				OutResult.Error = FString::Printf(
					TEXT("mode=replace delete is sandbox-only; Magecraft path '%s' already exists — use adapt_niagara_effect or submit_niagara_graph."),
					*CreatedPath);
				return false;
			}
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
				TEXT("Asset already exists at '%s'. Use envelope mode 'replace' for idempotent probes under %s, or adapt_niagara_effect on Magecraft."),
				*CreatedPath,
				*UeremcpNiagaraPaths::AllowedMutateRootsDescription());
			return false;
		}
	}
	else if (bReplaceMode)
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.replace_no_existing_asset"));
	}

	const bool bVariation = !Spec.BaseSystemPath.IsEmpty();
	if (bVariation)
	{
		OutResult.InheritedAssetPath = Spec.BaseSystemPath;
	}
	FString TemplatePath = bVariation
		? Spec.BaseSystemPath
		: (Spec.TemplateSystemPath.IsEmpty()
			? FString(GLoopingSystemTemplate)
			: Spec.TemplateSystemPath);
	const bool bPrecipitationEffect =
		Spec.EffectType.Equals(TEXT("precipitation"), ESearchCase::IgnoreCase)
		|| Spec.EffectType.Equals(TEXT("rain"), ESearchCase::IgnoreCase)
		|| Spec.EffectType.Equals(TEXT("weather"), ESearchCase::IgnoreCase);
	if (!bVariation
		&& (Spec.EffectType.Equals(TEXT("projectile"), ESearchCase::IgnoreCase) || bPrecipitationEffect)
		&& TemplatePath.Equals(GMinimalSystemTemplate, ESearchCase::IgnoreCase))
	{
		// MinimalLightweight resolves to Once + zero loop duration and completes before
		// stateful emitter spawn modules execute. FountainLightweight supplies a looping
		// system lifecycle; its template emitter is removed by PrepareRuntimeScaffold.
		// [VERIFIED-RUNTIME: UEREMCP.Niagara.Create.PocBParticlesSpawn pre-fix:
		//  resolved loopBehavior=Once, loopDuration=0, total_spawned_particles=0]
		// Precipitation/rain likewise requires Infinite loop for camera-follow weather.
		TemplatePath = GLoopingSystemTemplate;
	}

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

	if (bVariation)
	{
		FNiagaraExt_SystemSummary BaseSummary;
		UNiagaraExternalEditUtilities::GetSystemSummary(System, BaseSummary, Context);
		++OutResult.InternalOperations;
		if (Context.HasErrors())
		{
			OutResult.Error = ContextErrorsToString(Context);
			return false;
		}
		for (const FNiagaraExt_EmitterSummary& Emitter : BaseSummary.Emitters)
		{
			OutResult.EmittersInherited.Add(Emitter.EmitterName.ToString());
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.inherit_base_system_emitters"));
	}
	else if (!PrepareRuntimeScaffold(
		System,
		Context,
		OutResult.InternalOperations,
		OutResult.ChecksPerformed,
		OutResult.Error))
	{
		return false;
	}

	TArray<FUeremcpNiagaraEmitterPlan> Plans = Spec.Emitters;
	if (Plans.Num() == 0)
	{
		for (const FString& Role : Spec.ComponentRoles)
		{
			FUeremcpNiagaraEmitterPlan Plan;
			Plan.Role = Role;
			Plan.Name = RoleToEmitterName(Role);
			Plan.TemplatePath = ResolveEmitterTemplatePath(Role);
			Plans.Add(Plan);
		}
	}

	int32 CustomStackCount = 0;
	for (const FUeremcpNiagaraEmitterPlan& Plan : Plans)
	{
		UNiagaraEmitter* EmitterTemplate = Cast<UNiagaraEmitter>(LoadSoftPath(Plan.TemplatePath));
		if (!EmitterTemplate)
		{
			OutResult.Error = FString::Printf(
				TEXT("Could not load emitter template '%s' for role '%s' / name '%s'."),
				*Plan.TemplatePath,
				*Plan.Role,
				*Plan.Name);
			return false;
		}

		FNiagaraExt_EmitterTopology Topology;
		// [VERIFIED: NiagaraExternalSystemEditorUtilities.h:1361 AddEmitter]
		UNiagaraExternalEditUtilities::AddEmitter(
			EmitterTemplate,
			FName(*Plan.Name),
			Topology,
			Context);
		++OutResult.InternalOperations;

		if (Context.HasErrors())
		{
			OutResult.Error = ContextErrorsToString(Context);
			return false;
		}

		OutResult.EmittersAdded.Add(Plan.Name);

		// LLM-defined stacks: Minimal substrate + AddModule per modules[] (+ inputs).
		if (Plan.Modules.Num() > 0)
		{
			if (!ApplyCustomEmitterStack(System, Context, Plan, OutResult))
			{
				return false;
			}
			++CustomStackCount;
		}
		else
		{
			// Role/template shortcut path — still apply first-class Emitter Properties.
			FUeremcpNiagaraEmitterPropertyPlan Props;
			Props.SimTarget = Plan.SimTarget;
			Props.bHasEnabled = Plan.bHasEnabled;
			Props.bEnabled = Plan.bEnabled;
			Props.LifeCycleMode = Plan.LifeCycleMode;
			Props.LoopBehavior = Plan.LoopBehavior;
			Props.LoopDuration = Plan.LoopDuration;
			Props.InactiveResponse = Plan.InactiveResponse;
			if (Props.HasAny())
			{
				TArray<FString> AppliedProps;
				FUeremcpNiagaraEmitterProperties::ApplyAll(
					System,
					Context,
					Plan.Name,
					Props,
					OutResult.InternalOperations,
					AppliedProps,
					OutResult.LossyWarnings);
				if (AppliedProps.Num() > 0)
				{
					OutResult.ChecksPerformed.Add(TEXT("niagara.emitter_properties_write"));
				}
			}
		}
	}

	if (CustomStackCount > 0)
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.add_emitters_custom_module_stacks"));
		OutResult.ChecksPerformed.Add(TEXT("niagara.add_modules_from_emitters_json"));
	}
	OutResult.ChecksPerformed.Add(TEXT("niagara.add_emitters_from_plan"));
	TArray<FString> VariationEmitterNames = OutResult.EmittersInherited;
	VariationEmitterNames.Append(OutResult.EmittersAdded);
	const TArray<FString>& TargetEmitterNames = bVariation
		? VariationEmitterNames
		: OutResult.EmittersAdded;

	FUeremcpNiagaraMaterialBinding::NormalizeMeshRendererOverrideFlags(
		System,
		Context,
		OutResult.InternalOperations);

	// Stronger default mist density when precipitation/mist roles are present and
	// the agent did not pass density knobs (IW-005 field: ground mist too thin).
	TSharedPtr<FJsonObject> EffectiveParams = Spec.Parameters;
	const bool bHasMistRole = Spec.ComponentRoles.ContainsByPredicate(
		[](const FString& Role)
		{
			return Role.Equals(TEXT("mist"), ESearchCase::IgnoreCase)
				|| Role.Equals(TEXT("fog"), ESearchCase::IgnoreCase);
		});
	const bool bPrecipitation =
		Spec.EffectType.Equals(TEXT("precipitation"), ESearchCase::IgnoreCase)
		|| Spec.EffectType.Equals(TEXT("rain"), ESearchCase::IgnoreCase)
		|| Spec.EffectType.Equals(TEXT("weather"), ESearchCase::IgnoreCase)
		|| Spec.EffectType.Equals(TEXT("mist"), ESearchCase::IgnoreCase);
	if ((bPrecipitation || bHasMistRole)
		&& (!EffectiveParams.IsValid()
			|| (!EffectiveParams->HasField(TEXT("density"))
				&& !EffectiveParams->HasField(TEXT("mist_density"))
				&& !EffectiveParams->HasField(TEXT("intensity")))))
	{
		if (!EffectiveParams.IsValid())
		{
			EffectiveParams = MakeShared<FJsonObject>();
		}
		else
		{
			EffectiveParams = MakeShared<FJsonObject>(*EffectiveParams);
		}
		EffectiveParams->SetNumberField(TEXT("density"), 2.5);
		EffectiveParams->SetNumberField(TEXT("intensity"), 3.0);
		OutResult.ChecksPerformed.Add(TEXT("niagara.default_mist_density"));
	}

	ApplySpecificationParameters(
		System,
		Context,
		EffectiveParams,
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

	if (!ApplyParticleColor(
		System,
		Context,
		TargetEmitterNames,
		Spec.Parameters,
		OutResult.InternalOperations,
		OutResult.ChecksPerformed,
		OutResult.Error))
	{
		return false;
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
			TargetEmitterNames,
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

	PhaseTimer.MarkPhase(TEXT("asset_creation"));

	if (Request.bCompile)
	{
		FNiagaraExt_SystemCompileState CompileState;
		const int32 TimeoutSeconds = Request.TimeoutMs > 0 ? FMath::Max(1, Request.TimeoutMs / 1000) : 120;
		const FUeremcpNiagaraCompileAwaitResult AwaitResult =
			FUeremcpNiagaraCompileAwait::AwaitCompile(System, Context, TimeoutSeconds, CompileState);
		++OutResult.InternalOperations;
		OutResult.ChecksPerformed.Add(TEXT("niagara.compile_await"));

		if (AwaitResult.bObservedViaScriptState)
		{
			OutResult.ChecksPerformed.Add(TEXT("niagara.compile_await_observed_via_script_state"));
		}
		if (AwaitResult.bLiveEnginePumpSkipped)
		{
			OutResult.ChecksPerformed.Add(TEXT("niagara.compile_await_live_engine_pump_skipped"));
		}
		if (AwaitResult.bActiveQueueNotDrained)
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.compile_active_queue_not_drained"));
		}
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
			OutResult.Error = TEXT("Niagara compile finished with errors.");
			return false;
		}

		if (CompileState.bIsCompiling || !AwaitResult.bAwaited)
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.compile_complete_within_timeout"));
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.compile_await"));
	}

	PhaseTimer.MarkPhase(TEXT("compilation"));

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

	PhaseTimer.MarkPhase(TEXT("save"));

	// Structural re-read: confirm emitters (+ custom modules) exist (honest success gate).
	{
		FNiagaraExt_SystemSummary VerifySummary;
		UNiagaraExternalEditUtilities::GetSystemSummary(System, VerifySummary, Context);
		++OutResult.InternalOperations;
		TSet<FString> LiveNames;
		for (const FNiagaraExt_EmitterSummary& Emitter : VerifySummary.Emitters)
		{
			LiveNames.Add(Emitter.EmitterName.ToString());
		}
		TArray<FString> Missing;
		for (const FString& Expected : OutResult.EmittersAdded)
		{
			if (!LiveNames.Contains(Expected))
			{
				Missing.Add(Expected);
			}
		}
		if (Missing.Num() > 0)
		{
			OutResult.Error = FString::Printf(
				TEXT("Structural re-read failed: missing emitter(s) after AddEmitter: %s"),
				*FString::Join(Missing, TEXT(", ")));
			OutResult.ChecksSkipped.Add(TEXT("niagara.structural_re_read"));
			return false;
		}
		if (OutResult.EmittersAdded.Num() == 0 && !bVariation)
		{
			OutResult.Error = TEXT(
				"Structural re-read: create produced zero emitters. "
				"Pass specification.emitters[{name,modules[]}] or role shortcuts.");
			OutResult.ChecksSkipped.Add(TEXT("niagara.structural_re_read"));
			return false;
		}

		// Re-inspect custom stacks: each planned module name must appear on some script stack.
		for (const FUeremcpNiagaraEmitterPlan& Plan : Plans)
		{
			if (Plan.Modules.Num() == 0)
			{
				continue;
			}
			FNiagaraExt_StackItemReference EmitterRef(System, FName(*Plan.Name));
			FNiagaraExt_EmitterTopology LiveTopo;
			UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, LiveTopo, Context);
			++OutResult.InternalOperations;
			TSet<FString> LiveModuleNames;
			auto Collect = [&LiveModuleNames](const FNiagaraExt_ScriptStackTopology& Stack)
			{
				for (const FNiagaraExt_ModuleTopology& M : Stack.Modules)
				{
					LiveModuleNames.Add(M.ModuleName.ToString());
				}
			};
			Collect(LiveTopo.EmitterSpawnScript);
			Collect(LiveTopo.EmitterUpdateScript);
			Collect(LiveTopo.ParticleSpawnScript);
			Collect(LiveTopo.ParticleUpdateScript);
			for (const FUeremcpNiagaraModulePlan& Mod : Plan.Modules)
			{
				const FString AssetLeaf = FPackageName::GetLongPackageAssetName(Mod.AssetPath);
				bool bFound = LiveModuleNames.Contains(Mod.Name) || LiveModuleNames.Contains(AssetLeaf);
				if (!bFound)
				{
					for (const FString& Live : LiveModuleNames)
					{
						if (Live.Contains(Mod.Name)
							|| (!AssetLeaf.IsEmpty() && Live.Contains(AssetLeaf)))
						{
							bFound = true;
							break;
						}
					}
				}
				if (!bFound)
				{
					OutResult.LossyWarnings.Add(FString::Printf(
						TEXT("structural re-read: module '%s' not found by name on emitter '%s' "
							 "(AddModule may rename; check Inspect topology_summary)"),
						*Mod.Name,
						*Plan.Name));
				}
			}
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.structural_re_read"));
	}
	OutResult.ChecksSkipped.Add(TEXT("niagara.runtime_smoke_test"));

	OutResult.CreatedAssetPath = CreatedPath;
	OutResult.bSuccess = true;
	const FString PathKind = UeremcpNiagaraPaths::IsAllowedMagecraftPath(CreatedPath)
		? TEXT("Magecraft")
		: TEXT("sandbox");
	if (OutResult.bReplacedExisting)
	{
		OutResult.Summary = FString::Printf(
			TEXT("Replaced Niagara %s effect '%s' (effect_type=%s): inherited %d emitter(s), added %d emitter(s), "
				 "%d module(s) via AddModule, %d renderer(s), %d user variable(s), %d verified material binding(s). "
				 "round_trip_supported=false = hash not proven — authoring DID add emitters/modules."),
			*PathKind,
			*CreatedPath,
			*Spec.EffectType,
			OutResult.EmittersInherited.Num(),
			OutResult.EmittersAdded.Num(),
			OutResult.ModulesAdded.Num(),
			OutResult.RenderersAdded.Num(),
			OutResult.UserVariablesAdded.Num(),
			OutResult.MaterialBindings.RendererBindingsVerified.Num());
	}
	else
	{
		OutResult.Summary = FString::Printf(
			TEXT("Created Niagara %s effect '%s' (effect_type=%s): inherited %d emitter(s), added %d emitter(s), "
				 "%d module(s) via AddModule, %d renderer(s), %d user variable(s), %d verified material binding(s). "
				 "round_trip_supported=false = hash not proven — authoring DID add emitters/modules."),
			*PathKind,
			*CreatedPath,
			*Spec.EffectType,
			OutResult.EmittersInherited.Num(),
			OutResult.EmittersAdded.Num(),
			OutResult.ModulesAdded.Num(),
			OutResult.RenderersAdded.Num(),
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
