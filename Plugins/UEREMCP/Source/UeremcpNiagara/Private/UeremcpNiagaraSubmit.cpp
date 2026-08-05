// UEREMCP — submit edited Niagara graph JSON (WS-07).

#include "UeremcpNiagaraSubmit.h"

#include "UeremcpNiagaraCompileAwait.h"
#include "UeremcpNiagaraEmitterProperties.h"
#include "UeremcpNiagaraInspect.h"
#include "UeremcpNiagaraModuleResolve.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraRoleNames.h"
#include "UeremcpNiagaraStackInputs.h"

#include "Misc/PackageName.h"
#include "NiagaraEmitter.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraVariant.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
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
		if (!Loaded)
		{
			Loaded = FSoftObjectPath(AssetPath).TryLoad();
		}
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Loaded);
		if (!System)
		{
			OutError = FString::Printf(
				TEXT("submit_niagara_graph could not load NiagaraSystem at '%s'."),
				*AssetPath);
			return nullptr;
		}
		return System;
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

	bool IsReplaceMode(const FString& Mode)
	{
		return Mode.Equals(TEXT("replace"), ESearchCase::IgnoreCase)
			|| Mode.Equals(TEXT("rebuild_from_specification"), ESearchCase::IgnoreCase);
	}

	TSharedPtr<FJsonObject> GetNiagaraExt(const TSharedPtr<FJsonObject>& Graph)
	{
		if (!Graph.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* ExtRoot = nullptr;
		if (!Graph->TryGetObjectField(TEXT("extensions"), ExtRoot) || !ExtRoot || !(*ExtRoot).IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Niagara = nullptr;
		if (!(*ExtRoot)->TryGetObjectField(TEXT("niagara"), Niagara) || !Niagara || !(*Niagara).IsValid())
		{
			return nullptr;
		}
		return *Niagara;
	}

	struct FDesiredModule
	{
		FString ModuleName;
		FString ModuleScriptPath;
		FString PrimitiveId;
		bool bEnabled = true;
		bool bIsSetParameters = false;
		int32 StackIndex = INDEX_NONE;
		TSharedPtr<FJsonObject> Inputs;
	};

	struct FDesiredStack
	{
		FString EmitterName;
		FString ScriptUsage;
		TArray<FDesiredModule> Modules;
	};

	struct FDesiredEmitter
	{
		FString EmitterName;
		FString Role;
		FString TemplatePath;
		bool bEnabled = true;
		bool bHasEnabled = false;
		bool bUseMinimalSubstrate = false;
		TArray<TPair<int32, FString>> RendererMaterials;
		TArray<FDesiredModule> AuthoredModules;
		FUeremcpNiagaraEmitterPropertyPlan Properties;
	};

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

	void ParseDesiredFromGraphs(
		const TArray<TSharedPtr<FJsonObject>>& Graphs,
		TArray<FDesiredStack>& OutStacks,
		TArray<FDesiredEmitter>& OutEmitters,
		TArray<TSharedPtr<FJsonObject>>& OutUserVariables)
	{
		OutStacks.Reset();
		OutEmitters.Reset();
		OutUserVariables.Reset();

		for (const TSharedPtr<FJsonObject>& Graph : Graphs)
		{
			if (!Graph.IsValid())
			{
				continue;
			}

			FString GraphType;
			Graph->TryGetStringField(TEXT("graph_type"), GraphType);

			if (GraphType == TEXT("NiagaraSystemGraph"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Vars = nullptr;
				if (Graph->TryGetArrayField(TEXT("variables"), Vars) && Vars)
				{
					for (const TSharedPtr<FJsonValue>& V : *Vars)
					{
						if (V.IsValid() && V->Type == EJson::Object)
						{
							OutUserVariables.Add(V->AsObject());
						}
					}
				}
				if (TSharedPtr<FJsonObject> Niagara = GetNiagaraExt(Graph))
				{
					const TArray<TSharedPtr<FJsonValue>>* ExtVars = nullptr;
					if (Niagara->TryGetArrayField(TEXT("user_parameters"), ExtVars) && ExtVars
						&& OutUserVariables.Num() == 0)
					{
						for (const TSharedPtr<FJsonValue>& V : *ExtVars)
						{
							if (V.IsValid() && V->Type == EJson::Object)
							{
								OutUserVariables.Add(V->AsObject());
							}
						}
					}
				}
				continue;
			}

			if (GraphType == TEXT("NiagaraEmitterGraph"))
			{
				FDesiredEmitter Emitter;
				if (TSharedPtr<FJsonObject> Niagara = GetNiagaraExt(Graph))
				{
					Niagara->TryGetStringField(TEXT("emitter_name"), Emitter.EmitterName);
					Niagara->TryGetStringField(TEXT("role"), Emitter.Role);
					Niagara->TryGetStringField(TEXT("template_path"), Emitter.TemplatePath);
					if (Emitter.TemplatePath.IsEmpty())
					{
						Niagara->TryGetStringField(TEXT("emitter_template"), Emitter.TemplatePath);
					}
					if (Niagara->HasField(TEXT("bEnabled")))
					{
						Emitter.bEnabled = Niagara->GetBoolField(TEXT("bEnabled"));
						Emitter.bHasEnabled = true;
					}
					FUeremcpNiagaraEmitterProperties::ParseFromJsonObject(Niagara, Emitter.Properties);
					if (Emitter.Properties.bHasEnabled)
					{
						Emitter.bEnabled = Emitter.Properties.bEnabled;
						Emitter.bHasEnabled = true;
					}
					const TArray<TSharedPtr<FJsonValue>>* Renderers = nullptr;
					if (Niagara->TryGetArrayField(TEXT("renderers"), Renderers) && Renderers)
					{
						for (const TSharedPtr<FJsonValue>& R : *Renderers)
						{
							const TSharedPtr<FJsonObject> RObj = R.IsValid() ? R->AsObject() : nullptr;
							if (!RObj.IsValid())
							{
								continue;
							}
							FString MaterialPath;
							if (RObj->TryGetStringField(TEXT("material_path"), MaterialPath)
								&& !MaterialPath.IsEmpty())
							{
								const int32 Index = static_cast<int32>(
									RObj->HasField(TEXT("renderer_index"))
										? RObj->GetNumberField(TEXT("renderer_index"))
										: 0);
								Emitter.RendererMaterials.Add(TPair<int32, FString>(Index, MaterialPath));
							}
						}
					}
				}
				if (Emitter.EmitterName.IsEmpty())
				{
					Graph->TryGetStringField(TEXT("graph_name"), Emitter.EmitterName);
				}
				if (Emitter.EmitterName.IsEmpty() && !Emitter.Role.IsEmpty())
				{
					Emitter.EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Emitter.Role);
				}
				if (!Emitter.EmitterName.IsEmpty())
				{
					OutEmitters.Add(Emitter);
				}
				continue;
			}

			if (GraphType != TEXT("NiagaraModuleStack"))
			{
				continue;
			}

			FDesiredStack Stack;
			if (TSharedPtr<FJsonObject> Niagara = GetNiagaraExt(Graph))
			{
				Niagara->TryGetStringField(TEXT("emitter_name"), Stack.EmitterName);
				Niagara->TryGetStringField(TEXT("script_usage"), Stack.ScriptUsage);
			}
			if (Stack.ScriptUsage.IsEmpty())
			{
				Graph->TryGetStringField(TEXT("graph_name"), Stack.ScriptUsage);
			}

			const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
			if (Graph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
			{
				for (const TSharedPtr<FJsonValue>& NodeVal : *Nodes)
				{
					const TSharedPtr<FJsonObject> Node = NodeVal.IsValid() ? NodeVal->AsObject() : nullptr;
					if (!Node.IsValid())
					{
						continue;
					}
					FString SemanticType;
					Node->TryGetStringField(TEXT("semantic_type"), SemanticType);
					if (!SemanticType.IsEmpty() && SemanticType != TEXT("niagara_module"))
					{
						continue;
					}

					FDesiredModule Mod;
					Node->TryGetStringField(TEXT("title"), Mod.ModuleName);
					if (Node->HasField(TEXT("enabled")))
					{
						Mod.bEnabled = Node->GetBoolField(TEXT("enabled"));
					}
					const TSharedPtr<FJsonObject>* Props = nullptr;
					if (Node->TryGetObjectField(TEXT("properties"), Props) && Props && (*Props).IsValid())
					{
						(*Props)->TryGetStringField(TEXT("module_script"), Mod.ModuleScriptPath);
						(*Props)->TryGetStringField(TEXT("primitive_id"), Mod.PrimitiveId);
						if (Mod.ModuleScriptPath.IsEmpty())
						{
							(*Props)->TryGetStringField(TEXT("asset_path"), Mod.ModuleScriptPath);
						}
						if (Mod.ModuleScriptPath.IsEmpty() && !Mod.PrimitiveId.IsEmpty())
						{
							FString ResolveError;
							UeremcpNiagaraModuleResolve::ResolveModuleAssetPath(
								Mod.PrimitiveId, FString(), Mod.ModuleScriptPath, ResolveError);
						}
						if ((*Props)->HasField(TEXT("bIsSetParametersModule")))
						{
							Mod.bIsSetParameters = (*Props)->GetBoolField(TEXT("bIsSetParametersModule"));
						}
						if ((*Props)->HasField(TEXT("stack_index")))
						{
							Mod.StackIndex = static_cast<int32>((*Props)->GetNumberField(TEXT("stack_index")));
						}
						const TSharedPtr<FJsonObject>* InputsObj = nullptr;
						if ((*Props)->TryGetObjectField(TEXT("inputs"), InputsObj)
							&& InputsObj && (*InputsObj).IsValid())
						{
							Mod.Inputs = *InputsObj;
						}
					}
					if (!Mod.ModuleName.IsEmpty())
					{
						Stack.Modules.Add(Mod);
					}
				}
			}

			if (!Stack.EmitterName.IsEmpty() && !Stack.ScriptUsage.IsEmpty())
			{
				OutStacks.Add(Stack);
			}
		}
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

	FNiagaraExt_UserVariable MakeBoolUserVariable(const FName& ParamName, bool Value)
	{
		FNiagaraExt_UserVariable Var;
		Var.Name = ParamName;
		Var.Type = FNiagaraTypeDefinition::GetBoolDef();
		FNiagaraBool BoolValue;
		BoolValue.SetValue(Value);
		FNiagaraVariant Variant;
		Variant.SetBytesValue(Var.Type, BoolValue);
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

	bool TryMakeUserVariableFromJson(
		const TSharedPtr<FJsonObject>& VarObj,
		FNiagaraExt_UserVariable& OutVar,
		FString& OutSkipReason)
	{
		OutSkipReason.Reset();
		if (!VarObj.IsValid())
		{
			OutSkipReason = TEXT("invalid variable object");
			return false;
		}

		FString Name;
		if (!VarObj->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
		{
			OutSkipReason = TEXT("missing name");
			return false;
		}

		if (!VarObj->HasField(TEXT("default_value")))
		{
			OutSkipReason = TEXT("no default_value (inspect names-only entries are no-ops)");
			return false;
		}

		const TSharedPtr<FJsonValue> DefaultVal = VarObj->TryGetField(TEXT("default_value"));
		if (!DefaultVal.IsValid() || DefaultVal->IsNull())
		{
			OutSkipReason = TEXT("null default_value");
			return false;
		}

		FString TypeHint;
		VarObj->TryGetStringField(TEXT("type"), TypeHint);

		if (DefaultVal->Type == EJson::Number)
		{
			OutVar = MakeFloatUserVariable(FName(*Name), static_cast<float>(DefaultVal->AsNumber()));
			return true;
		}
		if (DefaultVal->Type == EJson::Boolean)
		{
			OutVar = MakeBoolUserVariable(FName(*Name), DefaultVal->AsBool());
			return true;
		}
		if (DefaultVal->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Arr = DefaultVal->AsArray();
			if (Arr.Num() >= 3)
			{
				FLinearColor Color;
				Color.R = static_cast<float>(Arr[0]->AsNumber());
				Color.G = static_cast<float>(Arr[1]->AsNumber());
				Color.B = static_cast<float>(Arr[2]->AsNumber());
				Color.A = Arr.Num() > 3 ? static_cast<float>(Arr[3]->AsNumber()) : 1.0f;
				OutVar = MakeColorUserVariable(FName(*Name), Color);
				return true;
			}
		}

		OutSkipReason = FString::Printf(
			TEXT("unsupported default_value shape for '%s' (type hint '%s')"),
			*Name,
			*TypeHint);
		return false;
	}

	bool ApplyRendererMaterial(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		int32 RendererIndex,
		const FString& MaterialPath,
		int32& InOutOps,
		FString& OutError)
	{
		OutError.Reset();
		if (!UeremcpNiagaraPaths::IsAllowedMaterialBindPath(MaterialPath)
			&& !MaterialPath.StartsWith(TEXT("/Niagara/"))
			&& !MaterialPath.StartsWith(TEXT("/Engine/")))
		{
			OutError = FString::Printf(
				TEXT("material_path '%s' outside allowed bind roots"),
				*MaterialPath);
			return false;
		}

		FNiagaraExt_StackItemReference RendererRef(System, FName(*EmitterName));
		RendererRef.RendererIndex = RendererIndex;

		FNiagaraExt_RendererData Data;
		UNiagaraExternalEditUtilities::GetRendererData(RendererRef, Data, Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			OutError = ContextErrorsToString(Context);
			Context.Errors.Reset();
			return false;
		}

		TSharedPtr<FJsonObject> Props;
		if (!Data.PropertyValues.IsEmpty())
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data.PropertyValues);
			FJsonSerializer::Deserialize(Reader, Props);
		}
		if (!Props.IsValid())
		{
			Props = MakeShared<FJsonObject>();
		}

		TSharedPtr<FJsonObject> MaterialRef = MakeShared<FJsonObject>();
		MaterialRef->SetStringField(TEXT("refPath"), MaterialPath);
		Props->SetObjectField(TEXT("Material"), MaterialRef);

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Props.ToSharedRef(), Writer);
		Data.PropertyValues = Serialized;

		UNiagaraExternalEditUtilities::SetRendererData(RendererRef, Data, Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			OutError = ContextErrorsToString(Context);
			Context.Errors.Reset();
			return false;
		}
		return true;
	}

	void ReconcileStack(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FDesiredStack& Desired,
		bool bAllowRemove,
		bool bDryRun,
		FUeremcpNiagaraSubmitResult& OutResult)
	{
		FNiagaraExt_StackItemReference EmitterRef(System, FName(*Desired.EmitterName));
		FNiagaraExt_EmitterTopology Topology;
		UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, Topology, Context);
		++OutResult.InternalOperations;
		if (Context.HasErrors())
		{
			OutResult.LossyWarnings.Add(FString::Printf(
				TEXT("GetEmitterTopology failed for '%s': %s"),
				*Desired.EmitterName,
				*ContextErrorsToString(Context)));
			Context.Errors.Reset();
			return;
		}

		const FNiagaraExt_ScriptStackTopology* LiveStack = nullptr;
		const FName ScriptName(*Desired.ScriptUsage);
		if (Topology.EmitterSpawnScript.ScriptName == ScriptName
			|| Desired.ScriptUsage == TEXT("EmitterSpawnScript"))
		{
			LiveStack = &Topology.EmitterSpawnScript;
		}
		else if (Topology.EmitterUpdateScript.ScriptName == ScriptName
			|| Desired.ScriptUsage == TEXT("EmitterUpdateScript"))
		{
			LiveStack = &Topology.EmitterUpdateScript;
		}
		else if (Topology.ParticleSpawnScript.ScriptName == ScriptName
			|| Desired.ScriptUsage == TEXT("ParticleSpawnScript"))
		{
			LiveStack = &Topology.ParticleSpawnScript;
		}
		else if (Topology.ParticleUpdateScript.ScriptName == ScriptName
			|| Desired.ScriptUsage == TEXT("ParticleUpdateScript"))
		{
			LiveStack = &Topology.ParticleUpdateScript;
		}

		if (!LiveStack)
		{
			OutResult.LossyWarnings.Add(FString::Printf(
				TEXT("script stack '%s' not found on emitter '%s'"),
				*Desired.ScriptUsage,
				*Desired.EmitterName));
			return;
		}

		TMap<FString, const FNiagaraExt_ModuleTopology*> LiveByName;
		TArray<FString> LiveOrder;
		for (const FNiagaraExt_ModuleTopology& Mod : LiveStack->Modules)
		{
			const FString Name = Mod.ModuleName.ToString();
			LiveByName.Add(Name, &Mod);
			LiveOrder.Add(Name);
		}

		TArray<FString> DesiredOrder;
		TSet<FString> DesiredNames;
		for (const FDesiredModule& Mod : Desired.Modules)
		{
			DesiredOrder.Add(Mod.ModuleName);
			DesiredNames.Add(Mod.ModuleName);
		}

		bool bOrderMismatch = false;
		if (DesiredOrder.Num() == LiveOrder.Num())
		{
			for (int32 i = 0; i < DesiredOrder.Num(); ++i)
			{
				if (DesiredOrder[i] != LiveOrder[i])
				{
					bOrderMismatch = true;
					break;
				}
			}
		}
		else if (DesiredOrder.Num() > 0 && LiveOrder.Num() > 0)
		{
			TSet<FString> LiveSet(LiveOrder);
			if (DesiredNames.Num() == LiveSet.Num())
			{
				bool bSameSet = true;
				for (const FString& N : DesiredNames)
				{
					if (!LiveSet.Contains(N))
					{
						bSameSet = false;
						break;
					}
				}
				bOrderMismatch = bSameSet && DesiredOrder != LiveOrder;
			}
		}
		if (bOrderMismatch)
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.module_reorder_without_readd"));
			OutResult.LossyWarnings.Add(FString::Printf(
				TEXT("%s/%s: module order differs; reorder requires remove+re-add (lossy)"),
				*Desired.EmitterName,
				*Desired.ScriptUsage));
		}

		for (const FDesiredModule& DesiredMod : Desired.Modules)
		{
			if (const FNiagaraExt_ModuleTopology* const* Found = LiveByName.Find(DesiredMod.ModuleName))
			{
				const FNiagaraExt_ModuleTopology* Live = *Found;
				if (Live->Enabled != DesiredMod.bEnabled)
				{
					const FString Key = FString::Printf(
						TEXT("%s/%s/%s.enabled=%s"),
						*Desired.EmitterName,
						*Desired.ScriptUsage,
						*DesiredMod.ModuleName,
						DesiredMod.bEnabled ? TEXT("true") : TEXT("false"));
					OutResult.PlannedChanges.Add(Key);
					if (!bDryRun)
					{
						FNiagaraExt_StackItemReference ModuleRef(
							System,
							FName(*Desired.EmitterName),
							FName(*Desired.ScriptUsage),
							FName(*DesiredMod.ModuleName));
						UNiagaraExternalEditUtilities::SetModuleEnabled(ModuleRef, DesiredMod.bEnabled, Context);
						++OutResult.InternalOperations;
						if (Context.HasErrors())
						{
							OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
							Context.Errors.Reset();
						}
						else
						{
							OutResult.ModulesEnabledChanged.Add(Key);
						}
					}
				}
			}
			else
			{
				if (DesiredMod.bIsSetParameters || DesiredMod.ModuleScriptPath.IsEmpty())
				{
					OutResult.LossyWarnings.Add(FString::Printf(
						TEXT("cannot add module '%s' on %s/%s without module_script (SetParameters recreate not supported)"),
						*DesiredMod.ModuleName,
						*Desired.EmitterName,
						*Desired.ScriptUsage));
					OutResult.ChecksSkipped.Add(TEXT("niagara.add_set_parameters_from_graph"));
					continue;
				}

				const FString Key = FString::Printf(
					TEXT("add %s/%s/%s (%s)"),
					*Desired.EmitterName,
					*Desired.ScriptUsage,
					*DesiredMod.ModuleName,
					*DesiredMod.ModuleScriptPath);
				OutResult.PlannedChanges.Add(Key);
				if (!bDryRun)
				{
					UNiagaraScript* Script = Cast<UNiagaraScript>(
						FSoftObjectPath(DesiredMod.ModuleScriptPath).TryLoad());
					if (!Script)
					{
						OutResult.LossyWarnings.Add(FString::Printf(
							TEXT("could not load module_script '%s'"),
							*DesiredMod.ModuleScriptPath));
						continue;
					}

					FNiagaraExt_StackItemReference LocationRef(
						System,
						FName(*Desired.EmitterName),
						FName(*Desired.ScriptUsage));
					FNiagaraExt_ModuleTopology Added;
					UNiagaraExternalEditUtilities::AddModule(LocationRef, Script, Added, Context);
					++OutResult.InternalOperations;
					if (Context.HasErrors())
					{
						OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
						Context.Errors.Reset();
					}
					else
					{
						OutResult.ModulesAdded.Add(Key);
						if (!DesiredMod.bEnabled)
						{
							FNiagaraExt_StackItemReference ModuleRef(
								System,
								FName(*Desired.EmitterName),
								FName(*Desired.ScriptUsage),
								Added.ModuleName);
							UNiagaraExternalEditUtilities::SetModuleEnabled(ModuleRef, false, Context);
							++OutResult.InternalOperations;
						}
						// Apply declarative inputs{} after AddModule (local + linked/DI/HLSL).
						if (DesiredMod.Inputs.IsValid() && DesiredMod.Inputs->Values.Num() > 0)
						{
							TArray<FString> AppliedInputs;
							FUeremcpNiagaraStackInputs::ApplyModuleInputs(
								System,
								Context,
								Desired.EmitterName,
								Desired.ScriptUsage,
								Added.ModuleName,
								DesiredMod.Inputs,
								OutResult.InternalOperations,
								OutResult.LossyWarnings,
								AppliedInputs);
						}
					}
				}
			}
		}

		if (bAllowRemove)
		{
			for (const FString& LiveName : LiveOrder)
			{
				if (DesiredNames.Contains(LiveName))
				{
					continue;
				}
				const FString Key = FString::Printf(
					TEXT("remove %s/%s/%s"),
					*Desired.EmitterName,
					*Desired.ScriptUsage,
					*LiveName);
				OutResult.PlannedChanges.Add(Key);
				if (!bDryRun)
				{
					FNiagaraExt_StackItemReference ModuleRef(
						System,
						FName(*Desired.EmitterName),
						FName(*Desired.ScriptUsage),
						FName(*LiveName));
					UNiagaraExternalEditUtilities::RemoveModule(ModuleRef, Context);
					++OutResult.InternalOperations;
					if (Context.HasErrors())
					{
						OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
						Context.Errors.Reset();
					}
					else
					{
						OutResult.ModulesRemoved.Add(Key);
					}
				}
			}
		}
	}
}

bool FUeremcpNiagaraSubmit::ParseSpecification(
	const TSharedPtr<FJsonObject>& Specification,
	FUeremcpNiagaraSubmitSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FUeremcpNiagaraSubmitSpec();
	OutError.Reset();
	if (!Specification.IsValid())
	{
		OutError = TEXT("submit_niagara_graph requires specification.graphs[] and/or specification.emitters[].");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	Specification->TryGetArrayField(TEXT("graphs"), Graphs);
	if (Graphs)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Graphs)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				OutError = TEXT("specification.graphs entries must be objects.");
				return false;
			}
			const TSharedPtr<FJsonObject> Graph = Value->AsObject();
			FString GraphType;
			if (!Graph->TryGetStringField(TEXT("graph_type"), GraphType) || GraphType.IsEmpty())
			{
				OutError = TEXT("each graph requires graph_type.");
				return false;
			}
			OutSpec.Graphs.Add(Graph);
		}
	}

	// Authoring shortcut: emitters[{name, modules[{primitive_id|asset_path, inputs}]}] —
	// synthesize NiagaraEmitterGraph + NiagaraModuleStack graphs in one request.
	const TArray<TSharedPtr<FJsonValue>>* EmittersArr = nullptr;
	if (Specification->TryGetArrayField(TEXT("emitters"), EmittersArr) && EmittersArr)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *EmittersArr)
		{
			const TSharedPtr<FJsonObject>* EmitterObj = nullptr;
			if (!Entry->TryGetObject(EmitterObj) || !EmitterObj || !(*EmitterObj).IsValid())
			{
				OutError = TEXT("specification.emitters[] entries must be objects.");
				return false;
			}
			FString EmitterName;
			(*EmitterObj)->TryGetStringField(TEXT("name"), EmitterName);
			FString Role;
			(*EmitterObj)->TryGetStringField(TEXT("role"), Role);
			FString TemplatePath;
			(*EmitterObj)->TryGetStringField(TEXT("template_path"), TemplatePath);
			if (TemplatePath.IsEmpty())
			{
				(*EmitterObj)->TryGetStringField(TEXT("emitter_template"), TemplatePath);
			}
			if (EmitterName.IsEmpty() && !Role.IsEmpty())
			{
				EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Role);
			}
			if (EmitterName.IsEmpty())
			{
				OutError = TEXT("specification.emitters[] requires name (or role).");
				return false;
			}

			TSharedPtr<FJsonObject> EmitterGraph = MakeShared<FJsonObject>();
			EmitterGraph->SetStringField(TEXT("graph_type"), TEXT("NiagaraEmitterGraph"));
			EmitterGraph->SetStringField(TEXT("graph_name"), EmitterName);
			TSharedPtr<FJsonObject> ExtRoot = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> Niagara = MakeShared<FJsonObject>();
			Niagara->SetStringField(TEXT("emitter_name"), EmitterName);
			if (!Role.IsEmpty())
			{
				Niagara->SetStringField(TEXT("role"), Role);
			}
			if (!TemplatePath.IsEmpty())
			{
				Niagara->SetStringField(TEXT("template_path"), TemplatePath);
			}
			else
			{
				// Custom stacks without role → Minimal substrate.
				Niagara->SetStringField(
					TEXT("template_path"),
					UeremcpNiagaraModuleResolve::MinimalEmitterTemplatePath());
			}
			if ((*EmitterObj)->HasField(TEXT("enabled")))
			{
				Niagara->SetBoolField(TEXT("bEnabled"), (*EmitterObj)->GetBoolField(TEXT("enabled")));
			}
			// Propagate first-class Emitter Properties into synthesized graph extensions.
			FString SimTarget;
			if ((*EmitterObj)->TryGetStringField(TEXT("sim_target"), SimTarget)
				|| (*EmitterObj)->TryGetStringField(TEXT("SimTarget"), SimTarget))
			{
				Niagara->SetStringField(TEXT("sim_target"), SimTarget);
			}
			const TSharedPtr<FJsonObject>* LifeCycle = nullptr;
			if ((*EmitterObj)->TryGetObjectField(TEXT("life_cycle"), LifeCycle)
				&& LifeCycle && (*LifeCycle).IsValid())
			{
				Niagara->SetObjectField(TEXT("life_cycle"), *LifeCycle);
			}
			double LoopDuration = 0.0;
			if ((*EmitterObj)->TryGetNumberField(TEXT("loop_duration"), LoopDuration))
			{
				Niagara->SetNumberField(TEXT("loop_duration"), LoopDuration);
			}
			FString Flat;
			if ((*EmitterObj)->TryGetStringField(TEXT("life_cycle_mode"), Flat))
			{
				Niagara->SetStringField(TEXT("life_cycle_mode"), Flat);
			}
			if ((*EmitterObj)->TryGetStringField(TEXT("loop_behavior"), Flat))
			{
				Niagara->SetStringField(TEXT("loop_behavior"), Flat);
			}
			if ((*EmitterObj)->TryGetStringField(TEXT("inactive_response"), Flat))
			{
				Niagara->SetStringField(TEXT("inactive_response"), Flat);
			}
			ExtRoot->SetObjectField(TEXT("niagara"), Niagara);
			EmitterGraph->SetObjectField(TEXT("extensions"), ExtRoot);
			OutSpec.Graphs.Add(EmitterGraph);

			const TArray<TSharedPtr<FJsonValue>>* ModulesArr = nullptr;
			if ((*EmitterObj)->TryGetArrayField(TEXT("modules"), ModulesArr) && ModulesArr)
			{
				TMap<FString, TArray<TSharedPtr<FJsonValue>>> NodesByScript;
				for (const TSharedPtr<FJsonValue>& ModVal : *ModulesArr)
				{
					FString PrimId;
					FString ModName;
					FString AssetPath;
					FString ScriptUsage;
					bool bEnabled = true;
					TSharedPtr<FJsonObject> Inputs;
					if (ModVal->TryGetString(ModName))
					{
						PrimId = ModName;
					}
					else
					{
						const TSharedPtr<FJsonObject>* ModObj = nullptr;
						if (!ModVal->TryGetObject(ModObj) || !ModObj || !(*ModObj).IsValid())
						{
							OutError = TEXT("emitters[].modules[] must be strings or objects.");
							return false;
						}
						(*ModObj)->TryGetStringField(TEXT("primitive_id"), PrimId);
						(*ModObj)->TryGetStringField(TEXT("name"), ModName);
						(*ModObj)->TryGetStringField(TEXT("asset_path"), AssetPath);
						if (AssetPath.IsEmpty())
						{
							(*ModObj)->TryGetStringField(TEXT("module_script"), AssetPath);
						}
						(*ModObj)->TryGetStringField(TEXT("script"), ScriptUsage);
						if (ScriptUsage.IsEmpty())
						{
							(*ModObj)->TryGetStringField(TEXT("script_usage"), ScriptUsage);
						}
						if ((*ModObj)->HasField(TEXT("enabled")))
						{
							bEnabled = (*ModObj)->GetBoolField(TEXT("enabled"));
						}
						const TSharedPtr<FJsonObject>* InputsObj = nullptr;
						if ((*ModObj)->TryGetObjectField(TEXT("inputs"), InputsObj)
							&& InputsObj && (*InputsObj).IsValid())
						{
							Inputs = *InputsObj;
						}
					}
					const FString Lookup = !PrimId.IsEmpty() ? PrimId : ModName;
					FString Resolved;
					FString ResolveError;
					if (!UeremcpNiagaraModuleResolve::ResolveModuleAssetPath(
						Lookup, AssetPath, Resolved, ResolveError))
					{
						OutError = FString::Printf(TEXT("emitter '%s': %s"), *EmitterName, *ResolveError);
						return false;
					}
					ScriptUsage = UeremcpNiagaraModuleResolve::NormalizeScriptUsage(ScriptUsage);
					if (ScriptUsage.IsEmpty())
					{
						ScriptUsage = UeremcpNiagaraModuleResolve::DefaultScriptUsageForModule(Lookup);
					}
					if (ModName.IsEmpty())
					{
						ModName = FPackageName::GetLongPackageAssetName(Resolved);
					}
					TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
					Node->SetStringField(TEXT("title"), ModName);
					Node->SetStringField(TEXT("semantic_type"), TEXT("niagara_module"));
					Node->SetBoolField(TEXT("enabled"), bEnabled);
					TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
					Props->SetStringField(TEXT("module_script"), Resolved);
					if (!PrimId.IsEmpty())
					{
						Props->SetStringField(TEXT("primitive_id"), PrimId);
					}
					if (Inputs.IsValid())
					{
						Props->SetObjectField(TEXT("inputs"), Inputs);
					}
					Node->SetObjectField(TEXT("properties"), Props);
					NodesByScript.FindOrAdd(ScriptUsage).Add(MakeShared<FJsonValueObject>(Node));
				}
				for (const TPair<FString, TArray<TSharedPtr<FJsonValue>>>& Pair : NodesByScript)
				{
					TSharedPtr<FJsonObject> StackGraph = MakeShared<FJsonObject>();
					StackGraph->SetStringField(TEXT("graph_type"), TEXT("NiagaraModuleStack"));
					StackGraph->SetStringField(TEXT("graph_name"), Pair.Key);
					TSharedPtr<FJsonObject> StackExtRoot = MakeShared<FJsonObject>();
					TSharedPtr<FJsonObject> StackNiagara = MakeShared<FJsonObject>();
					StackNiagara->SetStringField(TEXT("emitter_name"), EmitterName);
					StackNiagara->SetStringField(TEXT("script_usage"), Pair.Key);
					StackExtRoot->SetObjectField(TEXT("niagara"), StackNiagara);
					StackGraph->SetObjectField(TEXT("extensions"), StackExtRoot);
					StackGraph->SetArrayField(TEXT("nodes"), Pair.Value);
					OutSpec.Graphs.Add(StackGraph);
				}
			}
		}
	}

	if (OutSpec.Graphs.Num() == 0)
	{
		OutError = TEXT(
			"submit_niagara_graph requires specification.graphs and/or specification.emitters "
			"with at least one entry.");
		return false;
	}

	const TSharedPtr<FJsonObject>* Apply = nullptr;
	if (Specification->TryGetObjectField(TEXT("apply"), Apply) && Apply && (*Apply).IsValid())
	{
		if ((*Apply)->HasField(TEXT("modules")))
		{
			OutSpec.bApplyModules = (*Apply)->GetBoolField(TEXT("modules"));
		}
		if ((*Apply)->HasField(TEXT("user_parameters")))
		{
			OutSpec.bApplyUserParameters = (*Apply)->GetBoolField(TEXT("user_parameters"));
		}
		if ((*Apply)->HasField(TEXT("renderer_materials")))
		{
			OutSpec.bApplyRendererMaterials = (*Apply)->GetBoolField(TEXT("renderer_materials"));
		}
		if ((*Apply)->HasField(TEXT("emitter_enabled")))
		{
			OutSpec.bApplyEmitterEnabled = (*Apply)->GetBoolField(TEXT("emitter_enabled"));
		}
		if ((*Apply)->HasField(TEXT("emitter_properties")))
		{
			OutSpec.bApplyEmitterProperties = (*Apply)->GetBoolField(TEXT("emitter_properties"));
		}
		if ((*Apply)->HasField(TEXT("add_emitters")))
		{
			OutSpec.bApplyAddEmitters = (*Apply)->GetBoolField(TEXT("add_emitters"));
		}
	}

	return true;
}

bool FUeremcpNiagaraSubmit::Run(
	const FUeremcpRequest& Request,
	const FUeremcpNiagaraSubmitSpec& Spec,
	FUeremcpNiagaraSubmitResult& OutResult)
{
	OutResult = FUeremcpNiagaraSubmitResult();
	OutResult.AssetPath = Request.TargetAssetPath;

	if (!UeremcpNiagaraPaths::IsAllowedMutatePath(Request.TargetAssetPath))
	{
		OutResult.Error = UeremcpNiagaraPaths::MutateDeniedReason(Request.TargetAssetPath);
		return false;
	}

	if (Request.Mode.Equals(TEXT("delete"), ESearchCase::IgnoreCase))
	{
		OutResult.Error = TEXT(
			"submit_niagara_graph does not support mode=delete. "
			"Use mode=replace for stack rebuild (sandbox delete-recreate is create_niagara_effect only). "
			"Magecraft assets are never deleted.");
		return false;
	}

	const bool bReplace = IsReplaceMode(Request.Mode);
	if (bReplace && UeremcpNiagaraPaths::IsAllowedMagecraftPath(Request.TargetAssetPath))
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_magecraft_inplace_no_asset_delete"));
		OutResult.LossyWarnings.Add(TEXT(
			"mode=replace on Magecraft applies in-place stack reconciliation only — the UAsset is never deleted"));
	}

	FString LoadError;
	UNiagaraSystem* System = LoadExistingSystem(Request.TargetAssetPath, LoadError);
	if (!System)
	{
		OutResult.Error = LoadError;
		return false;
	}

	FNiagaraExternalEditContext Context(System);
	TArray<FDesiredStack> DesiredStacks;
	TArray<FDesiredEmitter> DesiredEmitters;
	TArray<TSharedPtr<FJsonObject>> DesiredUserVars;
	ParseDesiredFromGraphs(Spec.Graphs, DesiredStacks, DesiredEmitters, DesiredUserVars);

	OutResult.ChecksPerformed.Add(TEXT("niagara.submit_parse_graphs"));

	// Add missing emitters from template/role BEFORE module reconcile (stacks need the emitter).
	if (Spec.bApplyAddEmitters)
	{
		FNiagaraExt_SystemSummary LiveSummary;
		UNiagaraExternalEditUtilities::GetSystemSummary(System, LiveSummary, Context);
		++OutResult.InternalOperations;
		TSet<FString> LiveEmitterNames;
		for (const FNiagaraExt_EmitterSummary& E : LiveSummary.Emitters)
		{
			LiveEmitterNames.Add(E.EmitterName.ToString());
		}

		for (const FDesiredEmitter& Emitter : DesiredEmitters)
		{
			if (LiveEmitterNames.Contains(Emitter.EmitterName))
			{
				continue;
			}
			FString TemplatePath = Emitter.TemplatePath;
			if (TemplatePath.IsEmpty() && !Emitter.Role.IsEmpty())
			{
				TemplatePath = UeremcpNiagaraRoles::ResolveEmitterTemplatePath(Emitter.Role);
			}
			if (TemplatePath.IsEmpty())
			{
				// Custom / LLM stacks: clone Minimal then AddModule from module stacks.
				// [VERIFIED: DefaultNiagara.ini DefaultEmptyEmitter → Minimal]
				TemplatePath = UeremcpNiagaraModuleResolve::MinimalEmitterTemplatePath();
				OutResult.LossyWarnings.Add(FString::Printf(
					TEXT("emitter '%s': no role/template_path — using Minimal substrate for modules[]"),
					*Emitter.EmitterName));
			}

			const FString Key = FString::Printf(
				TEXT("add_emitter %s from %s"),
				*Emitter.EmitterName,
				*TemplatePath);
			OutResult.PlannedChanges.Add(Key);
			if (!Request.bDryRun)
			{
				UNiagaraEmitter* TemplateEmitter = Cast<UNiagaraEmitter>(LoadSoftPath(TemplatePath));
				if (!TemplateEmitter)
				{
					OutResult.LossyWarnings.Add(FString::Printf(
						TEXT("could not load emitter template '%s' for '%s'"),
						*TemplatePath,
						*Emitter.EmitterName));
					continue;
				}
				FNiagaraExt_EmitterTopology Topology;
				// [VERIFIED: NiagaraExternalSystemEditorUtilities — AddEmitter]
				UNiagaraExternalEditUtilities::AddEmitter(
					TemplateEmitter,
					FName(*Emitter.EmitterName),
					Topology,
					Context);
				++OutResult.InternalOperations;
				if (Context.HasErrors())
				{
					OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
					Context.Errors.Reset();
				}
				else
				{
					OutResult.EmittersAdded.Add(Emitter.EmitterName);
					LiveEmitterNames.Add(Emitter.EmitterName);
				}
			}
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_add_emitters"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.submit_add_emitters"));
	}

	if (Spec.bApplyModules)
	{
		for (const FDesiredStack& Stack : DesiredStacks)
		{
			ReconcileStack(
				System,
				Context,
				Stack,
				/*bAllowRemove=*/bReplace,
				Request.bDryRun,
				OutResult);
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_reconcile_modules"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.submit_reconcile_modules"));
	}

	if (Spec.bApplyUserParameters)
	{
		for (const TSharedPtr<FJsonObject>& VarObj : DesiredUserVars)
		{
			FNiagaraExt_UserVariable Var;
			FString SkipReason;
			if (!TryMakeUserVariableFromJson(VarObj, Var, SkipReason))
			{
				if (!SkipReason.IsEmpty() && !SkipReason.StartsWith(TEXT("no default_value")))
				{
					OutResult.LossyWarnings.Add(SkipReason);
				}
				continue;
			}
			const FString Key = FString::Printf(TEXT("user_param %s"), *Var.Name.ToString());
			OutResult.PlannedChanges.Add(Key);
			if (!Request.bDryRun)
			{
				UNiagaraExternalEditUtilities::AddUserVariable(System, Var, Context);
				++OutResult.InternalOperations;
				if (Context.HasErrors())
				{
					OutResult.LossyWarnings.Add(ContextErrorsToString(Context));
					Context.Errors.Reset();
				}
				else
				{
					OutResult.UserVariablesTouched.Add(Var.Name.ToString());
				}
			}
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_user_parameters"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.submit_user_parameters"));
	}

	if (Spec.bApplyRendererMaterials)
	{
		for (const FDesiredEmitter& Emitter : DesiredEmitters)
		{
			for (const TPair<int32, FString>& Mat : Emitter.RendererMaterials)
			{
				const FString Key = FString::Printf(
					TEXT("renderer %s[%d] → %s"),
					*Emitter.EmitterName,
					Mat.Key,
					*Mat.Value);
				OutResult.PlannedChanges.Add(Key);
				if (!Request.bDryRun)
				{
					FString MatError;
					if (ApplyRendererMaterial(
						System,
						Context,
						Emitter.EmitterName,
						Mat.Key,
						Mat.Value,
						OutResult.InternalOperations,
						MatError))
					{
						OutResult.RendererMaterialsApplied.Add(Key);
					}
					else
					{
						OutResult.LossyWarnings.Add(MatError.IsEmpty()
							? FString::Printf(TEXT("renderer material apply failed: %s"), *Key)
							: MatError);
						OutResult.ChecksSkipped.Add(TEXT("niagara.renderer_material_bindings"));
					}
				}
			}
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_renderer_materials"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.submit_renderer_materials"));
	}

	if (Spec.bApplyEmitterEnabled)
	{
		for (const FDesiredEmitter& Emitter : DesiredEmitters)
		{
			if (!Emitter.bHasEnabled)
			{
				continue;
			}
			FNiagaraExt_StackItemReference EmitterRef(System, FName(*Emitter.EmitterName));
			FNiagaraExt_EmitterSummary Summary;
			UNiagaraExternalEditUtilities::GetEmitterSummary(EmitterRef, Summary, Context);
			++OutResult.InternalOperations;
			if (Summary.bEnabled == Emitter.bEnabled)
			{
				continue;
			}
			const FString Key = FString::Printf(
				TEXT("%s.bEnabled=%s"),
				*Emitter.EmitterName,
				Emitter.bEnabled ? TEXT("true") : TEXT("false"));
			OutResult.PlannedChanges.Add(Key);
			if (!Request.bDryRun)
			{
				FUeremcpNiagaraEmitterPropertyPlan Plan;
				Plan.bHasEnabled = true;
				Plan.bEnabled = Emitter.bEnabled;
				TArray<FString> Applied;
				FUeremcpNiagaraEmitterProperties::ApplySimTargetAndEnabled(
					System,
					Context,
					Emitter.EmitterName,
					Plan,
					OutResult.InternalOperations,
					Applied,
					OutResult.LossyWarnings);
				OutResult.EmittersEnabledChanged.Append(Applied);
			}
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_emitter_enabled"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.submit_emitter_enabled"));
	}

	if (Spec.bApplyEmitterProperties)
	{
		for (const FDesiredEmitter& Emitter : DesiredEmitters)
		{
			FUeremcpNiagaraEmitterPropertyPlan Plan = Emitter.Properties;
			// Enabled already handled above when bApplyEmitterEnabled.
			Plan.bHasEnabled = false;
			if (!Plan.HasAny())
			{
				continue;
			}
			const FString Key = FString::Printf(
				TEXT("%s properties (sim_target=%s life_cycle=%s)"),
				*Emitter.EmitterName,
				*Plan.SimTarget,
				Plan.HasLifeCycleFields() ? TEXT("yes") : TEXT("no"));
			OutResult.PlannedChanges.Add(Key);
			if (!Request.bDryRun)
			{
				TArray<FString> Applied;
				FUeremcpNiagaraEmitterProperties::ApplyAll(
					System,
					Context,
					Emitter.EmitterName,
					Plan,
					OutResult.InternalOperations,
					Applied,
					OutResult.LossyWarnings);
				OutResult.EmitterPropertiesApplied.Append(Applied);
			}
		}
		OutResult.ChecksPerformed.Add(TEXT("niagara.submit_emitter_properties"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.submit_emitter_properties"));
	}

	OutResult.ChecksSkipped.Add(TEXT("niagara.script_graph_internals_write"));
	// Event handler stacks: FNiagaraExt_StackItemReference has no UsageId — ParticleEventScript
	// cannot be addressed via AddModule/GetScript (FindScriptGroup requires matching Guid).
	// [VERIFIED: NiagaraStackQuery.cpp:160-182 FindScriptGroup; StackItemReference.h ScriptName only]
	OutResult.ChecksSkipped.Add(TEXT("niagara.event_handler_stacks_write"));
	OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));

	if (Request.bDryRun)
	{
		OutResult.Summary = FString::Printf(
			TEXT("Dry-run submit_niagara_graph for '%s': %d planned change(s). "
				 "No mutation. round_trip_supported=false."),
			*Request.TargetAssetPath,
			OutResult.PlannedChanges.Num());
		OutResult.bSuccess = true;
		return true;
	}

	if (Request.bCompile)
	{
		FNiagaraExt_SystemCompileState CompileState;
		const FUeremcpNiagaraCompileAwaitResult AwaitResult =
			FUeremcpNiagaraCompileAwait::AwaitCompile(System, Context, 120, CompileState);
		++OutResult.InternalOperations;
		OutResult.ChecksPerformed.Add(TEXT("niagara.compile_await"));
		if (AwaitResult.bObservedViaScriptState)
		{
			OutResult.ChecksPerformed.Add(TEXT("compile_await_observed_via_script_state"));
		}
		if (AwaitResult.bLiveEnginePumpSkipped)
		{
			OutResult.ChecksPerformed.Add(TEXT("compile_await_live_engine_pump_skipped"));
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
			FUeremcpNiagaraCompileAwait::IsScriptDerivedCompileComplete(CompileState);
		OutResult.bCompiled = AwaitResult.bAwaited && bUpToDate && !CompileState.bHasErrors;
		if (!OutResult.bCompiled.Get(false))
		{
			OutResult.Error = FString::Printf(
				TEXT("submit_niagara_graph compile did not reach UpToDate without errors "
					 "(bAwaited=%s, bHasErrors=%s, bIsCompiling=%s, live_pump_skipped=%s)."),
				AwaitResult.bAwaited ? TEXT("true") : TEXT("false"),
				CompileState.bHasErrors ? TEXT("true") : TEXT("false"),
				CompileState.bIsCompiling ? TEXT("true") : TEXT("false"),
				AwaitResult.bLiveEnginePumpSkipped ? TEXT("true") : TEXT("false"));
			return false;
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.compile_await"));
	}

	if (Request.bSave)
	{
		FString SaveError;
		if (SaveSystemPackage(System, SaveError))
		{
			OutResult.bSaved = true;
			OutResult.ChecksPerformed.Add(TEXT("niagara.save_package"));
		}
		else
		{
			OutResult.bSaved = false;
			OutResult.Error = SaveError;
			return false;
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.save_package"));
	}

	if (Request.bValidate)
	{
		FUeremcpRequest InspectRequest = Request;
		InspectRequest.Action = TEXT("inspect_system");
		InspectRequest.ResponseDetail = TEXT("complete");
		InspectRequest.bDryRun = true;
		FUeremcpNiagaraInspectSpec InspectSpec;
		FUeremcpNiagaraInspectResult InspectResult;
		if (FUeremcpNiagaraInspect::Run(InspectRequest, InspectSpec, InspectResult))
		{
			OutResult.PostInspectGraphs = InspectResult.Graphs;
			OutResult.InternalOperations += InspectResult.InternalOperations;
			OutResult.ChecksPerformed.Add(TEXT("niagara.post_submit_inspect"));

			TSet<FString> DesiredEmitterNames;
			for (const FDesiredEmitter& E : DesiredEmitters)
			{
				DesiredEmitterNames.Add(E.EmitterName);
			}
			for (const FDesiredStack& S : DesiredStacks)
			{
				DesiredEmitterNames.Add(S.EmitterName);
			}
			bool bEmittersOk = true;
			for (const FString& Name : DesiredEmitterNames)
			{
				if (!InspectResult.EmitterNames.Contains(Name))
				{
					bEmittersOk = false;
					OutResult.LossyWarnings.Add(FString::Printf(
						TEXT("post-inspect missing emitter '%s'"), *Name));
				}
			}
			OutResult.bStructuralMatchAfter = bEmittersOk;
		}
		else
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.post_submit_inspect"));
			OutResult.LossyWarnings.Add(InspectResult.Error);
		}
	}

	const int32 Applied =
		OutResult.EmittersAdded.Num()
		+ OutResult.ModulesEnabledChanged.Num()
		+ OutResult.ModulesAdded.Num()
		+ OutResult.ModulesRemoved.Num()
		+ OutResult.UserVariablesTouched.Num()
		+ OutResult.RendererMaterialsApplied.Num()
		+ OutResult.EmittersEnabledChanged.Num();

	OutResult.Summary = FString::Printf(
		TEXT("Submitted Niagara graph to '%s': %d applied mutation(s) (%d emitter(s) added), %d lossy warning(s). "
			 "round_trip_supported=false means hash not proven — structural submit still applied."),
		*Request.TargetAssetPath,
		Applied,
		OutResult.EmittersAdded.Num(),
		OutResult.LossyWarnings.Num());
	OutResult.bSuccess = true;
	return true;
}
