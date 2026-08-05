// UEREMCP — Niagara inspect → graph.schema.json mapper (WS-07).

#include "UeremcpNiagaraInspect.h"

#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraDependencySurvey.h"
#include "UeremcpNiagaraEmitterProperties.h"
#include "UeremcpNiagaraGraphHash.h"
#include "UeremcpNiagaraInspectMapping.h"
#include "UeremcpNiagaraPaths.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraRendererProperties.h"

#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const TCHAR* GGraphSchemaVersion = TEXT("1.0");

	FString SimTargetToString(ENiagaraSimTarget SimTarget)
	{
		return SimTarget == ENiagaraSimTarget::GPUComputeSim
			? TEXT("GPUComputeSim")
			: TEXT("CPUSim");
	}

	FString CompileStatusToString(ENiagaraExt_ScriptCompileStatus Status)
	{
		if (const UEnum* Enum = StaticEnum<ENiagaraExt_ScriptCompileStatus>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Status));
		}
		return TEXT("Unknown");
	}

	TSharedPtr<FJsonObject> MakeFidelityObject()
	{
		TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
		Fidelity->SetBoolField(TEXT("round_trip_supported"), false);

		TArray<TSharedPtr<FJsonValue>> Lossy;
		for (const FString& Area : UeremcpNiagaraCapability::DefaultFidelityLossyAreas())
		{
			Lossy.Add(MakeShared<FJsonValueString>(Area));
		}
		Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
		return Fidelity;
	}

	TSharedPtr<FJsonObject> StackInputValueToExtensionJson(const FNiagaraExt_StackInputValue& Value)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		const UScriptStruct* Struct = Value.GetScriptStruct();
		if (!Struct)
		{
			Out->SetStringField(TEXT("mode"), TEXT("unsupported"));
			return Out;
		}

		const FString StructName = Struct->GetName();
		if (StructName.Contains(TEXT("Linked")))
		{
			Out->SetStringField(TEXT("mode"), TEXT("linked"));
			if (const FNiagaraExt_StackInputData_Linked* Linked = Value.GetPtr<FNiagaraExt_StackInputData_Linked>())
			{
				Out->SetStringField(TEXT("linked_variable"), Linked->LinkedVariable.Name.ToString());
			}
		}
		else if (StructName.Contains(TEXT("HlslExpression")))
		{
			Out->SetStringField(TEXT("mode"), TEXT("hlsl_expression"));
			if (const FNiagaraExt_StackInputData_HlslExpression* Hlsl = Value.GetPtr<FNiagaraExt_StackInputData_HlslExpression>())
			{
				Out->SetStringField(TEXT("hlsl_expression"), Hlsl->HlslExpression);
			}
		}
		else if (StructName.Contains(TEXT("DataInterface")))
		{
			Out->SetStringField(TEXT("mode"), TEXT("data_interface"));
			if (const FNiagaraExt_StackInputData_DataInterface* DI = Value.GetPtr<FNiagaraExt_StackInputData_DataInterface>())
			{
				// UE 5.8: FNiagaraExt_StackInputData_DataInterface exposes PropertyValues JSON only
				// [VERIFIED: NiagaraExternalSystemEditorUtilities.h:574-580]
				if (!DI->PropertyValues.IsEmpty())
				{
					TSharedPtr<FJsonObject> ParsedValues;
					const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DI->PropertyValues);
					if (FJsonSerializer::Deserialize(Reader, ParsedValues) && ParsedValues.IsValid())
					{
						Out->SetObjectField(TEXT("data_interface"), ParsedValues);
					}
					else
					{
						Out->SetStringField(TEXT("property_values_json"), DI->PropertyValues);
					}
				}
			}
		}
		else if (StructName.Contains(TEXT("DynamicInput")))
		{
			Out->SetStringField(TEXT("mode"), TEXT("dynamic_input"));
			if (const FNiagaraExt_StackInputData_DynamicInput* Dyn = Value.GetPtr<FNiagaraExt_StackInputData_DynamicInput>())
			{
				TSharedPtr<FJsonObject> Chain = MakeShared<FJsonObject>();
				if (Dyn->DynamicInputAsset)
				{
					Chain->SetStringField(TEXT("script"), Dyn->DynamicInputAsset->GetPathName());
				}
				Out->SetObjectField(TEXT("dynamic_chain"), Chain);
			}
		}
		else if (StructName.Contains(TEXT("Enum")))
		{
			Out->SetStringField(TEXT("mode"), TEXT("enum"));
			if (const FNiagaraExt_StackInputData_Enum* EnumVal = Value.GetPtr<FNiagaraExt_StackInputData_Enum>())
			{
				Out->SetStringField(TEXT("enum_value"), EnumVal->EnumName.ToString());
			}
		}
		else if (StructName.Contains(TEXT("Unsupported")))
		{
			Out->SetStringField(TEXT("mode"), TEXT("unsupported"));
		}
		else
		{
			Out->SetStringField(TEXT("mode"), TEXT("local"));
			TSharedPtr<FJsonObject> Local = MakeShared<FJsonObject>();
			Local->SetStringField(TEXT("struct"), Struct->GetPathName());
			Out->SetObjectField(TEXT("local_value"), Local);
		}
		return Out;
	}

	TSharedPtr<FJsonObject> BuildModuleStackGraph(
		const FString& AssetPath,
		const FString& EmitterName,
		const FNiagaraExt_ScriptStackTopology& Stack,
		const TMap<FName, const FNiagaraExt_ModuleInputValues*>& InputValuesByModule,
		bool bIncludeInputValues,
		bool bOmitNodes,
		int32& InOutModuleCount)
	{
		const FString ScriptUsage = Stack.ScriptName.ToString();
		const FString GraphId = FString::Printf(TEXT("%s::%s::%s"), *AssetPath, *EmitterName, *ScriptUsage);

		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("asset_path"), AssetPath);
		Graph->SetStringField(TEXT("graph_id"), GraphId);
		Graph->SetStringField(TEXT("graph_name"), ScriptUsage);
		Graph->SetStringField(TEXT("graph_type"), TEXT("NiagaraModuleStack"));
		Graph->SetStringField(TEXT("schema_version"), GGraphSchemaVersion);
		Graph->SetObjectField(TEXT("fidelity"), MakeFidelityObject());

		TSharedPtr<FJsonObject> Ext = MakeShared<FJsonObject>();
		Ext->SetStringField(TEXT("script_usage"), ScriptUsage);
		Ext->SetStringField(TEXT("emitter_name"), EmitterName);
		TSharedPtr<FJsonObject> NiagaraExt = MakeShared<FJsonObject>();
		NiagaraExt->SetObjectField(TEXT("niagara"), Ext);
		Graph->SetObjectField(TEXT("extensions"), NiagaraExt);

		TSharedPtr<FJsonObject> Semantic = MakeShared<FJsonObject>();
		Semantic->SetStringField(TEXT("purpose"), FString::Printf(TEXT("Niagara %s module stack"), *ScriptUsage));
		Semantic->SetNumberField(TEXT("node_count"), Stack.Modules.Num());
		Graph->SetObjectField(TEXT("semantic_summary"), Semantic);

		if (bOmitNodes)
		{
			return Graph;
		}

		TArray<TSharedPtr<FJsonValue>> Nodes;
		TArray<TSharedPtr<FJsonValue>> Links;
		TSharedPtr<FJsonObject> InputsExt = MakeShared<FJsonObject>();

		for (int32 ModuleIndex = 0; ModuleIndex < Stack.Modules.Num(); ++ModuleIndex)
		{
			const FNiagaraExt_ModuleTopology& Module = Stack.Modules[ModuleIndex];
			++InOutModuleCount;

			const FString NodeId = FString::Printf(TEXT("n%d"), ModuleIndex);
			const FString SemanticId = FString::Printf(
				TEXT("%s/%s/%s"), *EmitterName, *ScriptUsage, *Module.ModuleName.ToString());

			TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
			Node->SetStringField(TEXT("node_id"), NodeId);
			Node->SetStringField(TEXT("semantic_id"), SemanticId);
			Node->SetStringField(TEXT("node_class"), TEXT("UNiagaraStackModuleItem"));
			Node->SetStringField(TEXT("semantic_type"), TEXT("niagara_module"));
			Node->SetStringField(TEXT("title"), Module.ModuleName.ToString());
			Node->SetBoolField(TEXT("enabled"), Module.Enabled);

			TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
			Props->SetNumberField(TEXT("stack_index"), ModuleIndex);
			Props->SetBoolField(TEXT("bIsSetParametersModule"), Module.bIsSetParametersModule);
			if (Module.ModuleScript)
			{
				Props->SetStringField(TEXT("module_script"), Module.ModuleScript->GetPathName());
			}
			Node->SetObjectField(TEXT("properties"), Props);

			TArray<TSharedPtr<FJsonValue>> InputPins;
			for (const FNiagaraExt_StackInputTopology& Input : Module.Inputs)
			{
				TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
				Pin->SetStringField(TEXT("pin_id"), Input.Name.ToString());
				Pin->SetStringField(TEXT("name"), Input.Name.ToString());
				Pin->SetStringField(TEXT("direction"), TEXT("input"));
				InputPins.Add(MakeShared<FJsonValueObject>(Pin));
			}
			Node->SetArrayField(TEXT("input_pins"), InputPins);
			Nodes.Add(MakeShared<FJsonValueObject>(Node));

			if (ModuleIndex > 0)
			{
				TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
				Link->SetStringField(TEXT("from_node"), FString::Printf(TEXT("n%d"), ModuleIndex - 1));
				Link->SetStringField(TEXT("from_pin"), TEXT("exec_out"));
				Link->SetStringField(TEXT("to_node"), NodeId);
				Link->SetStringField(TEXT("to_pin"), TEXT("exec_in"));
				Link->SetStringField(TEXT("kind"), TEXT("exec"));
				Links.Add(MakeShared<FJsonValueObject>(Link));
			}

			if (bIncludeInputValues)
			{
				if (const FNiagaraExt_ModuleInputValues* const* Found = InputValuesByModule.Find(Module.ModuleName))
				{
					const FNiagaraExt_ModuleInputValues* ModuleValues = *Found;
					for (const FNiagaraExt_StackInputValueEntry& Entry : ModuleValues->Inputs)
					{
						InputsExt->SetObjectField(
							Entry.Name.ToString(),
							StackInputValueToExtensionJson(Entry.Value));
					}
				}
			}
		}

		Graph->SetArrayField(TEXT("nodes"), Nodes);
		Graph->SetArrayField(TEXT("links"), Links);
		if (InputsExt->Values.Num() > 0)
		{
			Ext->SetObjectField(TEXT("inputs"), InputsExt);
		}

		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Graph->SetObjectField(TEXT("diagnostics"), Diagnostics);
		return Graph;
	}

	UNiagaraSystem* LoadProbeSystem(const FString& AssetPath, FString& OutError)
	{
		FString Normalized = AssetPath;
		Normalized.TrimStartAndEndInline();
		if (!Normalized.StartsWith(TEXT("/Game/")))
		{
			OutError = TEXT("target.asset_path must be a /Game/ soft path");
			return nullptr;
		}

		UObject* Loaded = FSoftObjectPath(Normalized).TryLoad();
		if (!Loaded && !Normalized.Contains(TEXT(".")))
		{
			const FString BaseName = FPackageName::GetLongPackageAssetName(Normalized);
			Loaded = FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Normalized, *BaseName)).TryLoad();
		}

		UNiagaraSystem* System = Cast<UNiagaraSystem>(Loaded);
		if (!System)
		{
			OutError = FString::Printf(
				TEXT("Could not load NiagaraSystem at '%s' (missing asset or wrong type)."),
				*Normalized);
			return nullptr;
		}
		return System;
	}

	bool ValidateSystemForSummary(const UNiagaraSystem* System, FString& OutError)
	{
		if (!IsValid(System))
		{
			OutError = TEXT(
				"Niagara system is invalid or pending destruction; GetSystemSummary was not called.");
			return false;
		}

		const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterHandles.Num(); ++EmitterIndex)
		{
			const FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[EmitterIndex];
			const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
			if (!EmitterData)
			{
				continue;
			}

			const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
			for (int32 RendererIndex = 0; RendererIndex < Renderers.Num(); ++RendererIndex)
			{
				if (!IsValid(Renderers[RendererIndex]))
				{
					OutError = FString::Printf(
						TEXT(
							"Niagara system '%s' has an invalid renderer at emitter %d ('%s'), "
							"renderer %d; GetSystemSummary was not called."),
						*System->GetPathName(),
						EmitterIndex,
						*EmitterHandle.GetName().ToString(),
						RendererIndex);
					return false;
				}
			}
		}

		OutError.Reset();
		return true;
	}

	void AddTrace(TArray<TSharedPtr<FJsonValue>>& Trace, const FString& Step, bool bOk, const FString& Detail = FString())
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("step"), Step);
		Entry->SetBoolField(TEXT("ok"), bOk);
		if (!Detail.IsEmpty())
		{
			Entry->SetStringField(TEXT("detail"), Detail);
		}
		Trace.Add(MakeShared<FJsonValueObject>(Entry));
	}

	bool ShouldIncludeStack(const FUeremcpNiagaraInspectSpec& Spec, const FString& ScriptName)
	{
		return Spec.StackFilter.Num() == 0 || Spec.StackFilter.Contains(ScriptName);
	}

	bool ShouldIncludeEmitter(const FUeremcpNiagaraInspectSpec& Spec, const FString& EmitterName)
	{
		return Spec.EmitterFilter.Num() == 0 || Spec.EmitterFilter.Contains(EmitterName);
	}

	const FNiagaraExt_ScriptStackTopology* GetStackByName(
		const FNiagaraExt_EmitterTopology& Topology,
		const FName& ScriptName)
	{
		if (ScriptName == TEXT("EmitterSpawnScript"))
		{
			return &Topology.EmitterSpawnScript;
		}
		if (ScriptName == TEXT("EmitterUpdateScript"))
		{
			return &Topology.EmitterUpdateScript;
		}
		if (ScriptName == TEXT("ParticleSpawnScript"))
		{
			return &Topology.ParticleSpawnScript;
		}
		if (ScriptName == TEXT("ParticleUpdateScript"))
		{
			return &Topology.ParticleUpdateScript;
		}
		return nullptr;
	}
}

bool FUeremcpNiagaraInspect::ParseSpecification(
	const TSharedPtr<FJsonObject>& Spec,
	FUeremcpNiagaraInspectSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FUeremcpNiagaraInspectSpec();
	if (!Spec.IsValid())
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* Emitters = nullptr;
	if (Spec->TryGetArrayField(TEXT("emitters"), Emitters))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Emitters)
		{
			FString Name;
			if (Value->TryGetString(Name))
			{
				OutSpec.EmitterFilter.Add(Name);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Stacks = nullptr;
	if (Spec->TryGetArrayField(TEXT("stacks"), Stacks))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Stacks)
		{
			FString Name;
			if (Value->TryGetString(Name))
			{
				OutSpec.StackFilter.Add(Name);
			}
		}
	}

	auto ReadBool = [&](const TCHAR* Field, bool& OutField)
	{
		if (Spec->HasField(Field))
		{
			OutField = Spec->GetBoolField(Field);
		}
	};

	ReadBool(TEXT("include_input_values"), OutSpec.bIncludeInputValues);
	ReadBool(TEXT("include_renderers"), OutSpec.bIncludeRenderers);
	ReadBool(TEXT("include_dependencies"), OutSpec.bIncludeDependencies);
	ReadBool(TEXT("include_compile_state"), OutSpec.bIncludeCompileState);
	ReadBool(TEXT("include_stack_issues"), OutSpec.bIncludeStackIssues);

	Spec->TryGetStringField(TEXT("query"), OutSpec.Query);
	Spec->TryGetStringField(TEXT("asset_name"), OutSpec.AssetName);
	FString SearchRoot;
	if (Spec->TryGetStringField(TEXT("search_root"), SearchRoot) && !SearchRoot.IsEmpty())
	{
		OutSpec.SearchRoot = SearchRoot;
	}
	Spec->TryGetStringField(TEXT("response_detail"), OutSpec.ResponseDetail);

	OutError.Reset();
	return true;
}

bool FUeremcpNiagaraInspect::IsAllowedProbePath(const FString& AssetPath)
{
	// Legacy name: WRITE roots (sandbox + Magecraft). Sandbox-only stays on Paths::IsAllowedProbePath.
	return UeremcpNiagaraPaths::IsAllowedMutatePath(AssetPath);
}

bool FUeremcpNiagaraInspect::IsAllowedInspectPath(const FString& AssetPath)
{
	return UeremcpNiagaraPaths::IsAllowedInspectPath(AssetPath);
}

bool FUeremcpNiagaraInspect::ResolveTargetPath(
	const FUeremcpRequest& Request,
	const FUeremcpNiagaraInspectSpec& Spec,
	FString& OutAssetPath,
	FString& OutError,
	TArray<FString>& OutCandidates)
{
	OutCandidates.Reset();
	OutAssetPath.Reset();

	if (!Request.TargetAssetPath.IsEmpty())
	{
		if (!IsAllowedInspectPath(Request.TargetAssetPath))
		{
			OutError = FString::Printf(
				TEXT("inspect_system path '%s' is outside /Game."),
				*Request.TargetAssetPath);
			return false;
		}
		OutAssetPath = Request.TargetAssetPath;
		OutError.Reset();
		return true;
	}

	const FString Query = !Spec.Query.IsEmpty() ? Spec.Query : Spec.AssetName;
	if (Query.IsEmpty())
	{
		OutError = TEXT("inspect_system requires target.asset_path or specification.query / asset_name.");
		return false;
	}

	FString SearchRoot = Spec.SearchRoot.IsEmpty() ? FString(TEXT("/Game")) : Spec.SearchRoot;
	SearchRoot.TrimStartAndEndInline();
	if (!SearchRoot.StartsWith(TEXT("/")))
	{
		SearchRoot = TEXT("/") + SearchRoot;
	}
	if (!UeremcpNiagaraPaths::IsAllowedInspectPath(SearchRoot)
		&& !SearchRoot.Equals(TEXT("/Game"), ESearchCase::CaseSensitive))
	{
		// Allow /Game itself even when path is exactly the root token.
		if (!SearchRoot.StartsWith(TEXT("/Game")))
		{
			OutError = FString::Printf(TEXT("search_root '%s' is outside /Game."), *SearchRoot);
			return false;
		}
	}

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = ARM.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(*SearchRoot);
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	TArray<FString> AllCandidates;
	for (const FAssetData& Asset : Assets)
	{
		const FString Soft = Asset.GetSoftObjectPath().ToString();
		if (Soft.IsEmpty())
		{
			continue;
		}
		FString PackagePath = Soft;
		if (PackagePath.Contains(TEXT(".")))
		{
			PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
		}
		AllCandidates.AddUnique(PackagePath);
	}

	const FString QueryLower = Query.ToLower();
	TArray<FString> Exact;
	TArray<FString> Substring;
	for (const FString& Candidate : AllCandidates)
	{
		const FString Name = FPackageName::GetShortName(Candidate).ToLower();
		if (Name.Equals(QueryLower))
		{
			Exact.Add(Candidate);
		}
		else if (Name.Contains(QueryLower) || Candidate.ToLower().Contains(QueryLower))
		{
			Substring.Add(Candidate);
		}
	}

	const TArray<FString>& Matches = Exact.Num() > 0 ? Exact : Substring;
	OutCandidates = Matches;
	if (Matches.Num() == 0)
	{
		OutError = FString::Printf(
			TEXT("No UNiagaraSystem matching '%s' under %s."),
			*Query,
			*SearchRoot);
		return false;
	}
	if (Matches.Num() > 1)
	{
		OutError = FString::Printf(
			TEXT("Ambiguous Niagara query '%s' matched %d assets; pass target.asset_path or a more specific name."),
			*Query,
			Matches.Num());
		return false;
	}

	OutAssetPath = Matches[0];
	OutError.Reset();
	return true;
}

bool FUeremcpNiagaraInspect::ShouldSkipStackIssuesForProbe(const FString& AssetPath)
{
	// Sandbox only — Magecraft/production may collect GetStackIssues.
	return UeremcpNiagaraPaths::IsAllowedProbePath(AssetPath);
}

bool FUeremcpNiagaraInspect::Run(
	const FUeremcpRequest& Request,
	const FUeremcpNiagaraInspectSpec& Spec,
	FUeremcpNiagaraInspectResult& OutResult)
{
	OutResult = FUeremcpNiagaraInspectResult();

	if (!IsAllowedInspectPath(Request.TargetAssetPath))
	{
		OutResult.Error = FString::Printf(
			TEXT("inspect_system only assets under /Game (got '%s')."),
			*Request.TargetAssetPath);
		return false;
	}

	FString LoadError;
	UNiagaraSystem* System = LoadProbeSystem(Request.TargetAssetPath, LoadError);
	if (!System)
	{
		OutResult.Error = LoadError;
		AddTrace(OutResult.ExecutionTrace, TEXT("load_system"), false, LoadError);
		return false;
	}
	AddTrace(OutResult.ExecutionTrace, TEXT("load_system"), true, System->GetName());
	OutResult.ResolvedAssetPath = Request.TargetAssetPath;

	FString SummaryPreflightError;
	if (!ValidateSystemForSummary(System, SummaryPreflightError))
	{
		OutResult.Error = SummaryPreflightError;
		AddTrace(
			OutResult.ExecutionTrace,
			TEXT("preflight_get_system_summary"),
			false,
			SummaryPreflightError);
		return false;
	}
	AddTrace(
		OutResult.ExecutionTrace,
		TEXT("preflight_get_system_summary"),
		true,
		TEXT("system and renderer references are valid"));

	FNiagaraExternalEditContext Context(System);
	FNiagaraExt_SystemSummary Summary;
	UNiagaraExternalEditUtilities::GetSystemSummary(System, Summary, Context);
	++OutResult.InternalOperations;
	if (Context.HasErrors())
	{
		TArray<FString> ContextErrors;
		for (const FText& Error : Context.Errors)
		{
			ContextErrors.Add(Error.ToString());
		}
		OutResult.Error = FString::Printf(
			TEXT("GetSystemSummary failed for '%s': %s"),
			*Request.TargetAssetPath,
			*FString::Join(ContextErrors, TEXT("; ")));
		AddTrace(
			OutResult.ExecutionTrace,
			TEXT("get_system_summary"),
			false,
			OutResult.Error);
		return false;
	}
	AddTrace(
		OutResult.ExecutionTrace,
		TEXT("get_system_summary"),
		true,
		Summary.SystemName.ToString());

	const bool bOmitNodes = Request.ResponseDetail.Equals(TEXT("summary"), ESearchCase::IgnoreCase)
		|| Request.ResponseDetail.Equals(TEXT("minimal"), ESearchCase::IgnoreCase);

	// --- System graph ---
	const FString AssetPath = Request.TargetAssetPath;
	const FString SystemGraphId = FString::Printf(TEXT("%s::System"), *AssetPath);

	TSharedPtr<FJsonObject> SystemGraph = MakeShared<FJsonObject>();
	SystemGraph->SetStringField(TEXT("asset_path"), AssetPath);
	SystemGraph->SetStringField(TEXT("graph_id"), SystemGraphId);
	SystemGraph->SetStringField(TEXT("graph_name"), Summary.SystemName.ToString());
	SystemGraph->SetStringField(TEXT("graph_type"), TEXT("NiagaraSystemGraph"));
	SystemGraph->SetStringField(TEXT("schema_version"), GGraphSchemaVersion);
	SystemGraph->SetObjectField(TEXT("fidelity"), MakeFidelityObject());

	TArray<TSharedPtr<FJsonValue>> UserVars;
	for (const FNiagaraExt_UserVariable& Var : Summary.UserVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.Name.ToString());
		if (Var.Type.IsValid())
		{
			VarObj->SetStringField(TEXT("type"), Var.Type.GetName());
		}
		UserVars.Add(MakeShared<FJsonValueObject>(VarObj));
		OutResult.UserParameters.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	SystemGraph->SetArrayField(TEXT("variables"), UserVars);

	TArray<FString> EmitterSubgraphIds;
	TArray<TSharedPtr<FJsonValue>> SystemNodes;
	FUeremcpNiagaraDependencySurveyCounts DependencySurvey;

	// Topology summary at top of result — agents scan stacks without Python (docs/WHY.md).
	OutResult.TopologySummary = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> TopologyEmitterVals;

	for (const FNiagaraExt_EmitterSummary& EmitterSummary : Summary.Emitters)
	{
		const FString EmitterName = EmitterSummary.EmitterName.ToString();
		if (!ShouldIncludeEmitter(Spec, EmitterName))
		{
			continue;
		}

		const FString EmitterGraphId = FString::Printf(TEXT("%s::%s"), *AssetPath, *EmitterName);
		EmitterSubgraphIds.Add(EmitterGraphId);

		FNiagaraExt_StackItemReference EmitterRef(System, EmitterSummary.EmitterName);
		FNiagaraExt_EmitterTopology Topology;
		UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, Topology, Context);
		++OutResult.InternalOperations;

		if (Spec.bIncludeDependencies)
		{
			UeremcpNiagaraDependencySurvey::AccumulateFromEmitterTopology(Topology, DependencySurvey);
		}

		TMap<FName, const FNiagaraExt_ModuleInputValues*> InputValuesByModule;
		TArray<FNiagaraExt_ModuleInputValues> EmitterInputValues;
		if (Spec.bIncludeInputValues)
		{
			UNiagaraExternalEditUtilities::GetEmitterInputValues(EmitterRef, EmitterInputValues, Context);
			++OutResult.InternalOperations;
			for (const FNiagaraExt_ModuleInputValues& ModuleValues : EmitterInputValues)
			{
				InputValuesByModule.Add(ModuleValues.ModuleName, &ModuleValues);
			}
		}

		// Compact per-emitter topology row (always; independent of response_detail).
		{
			TSharedPtr<FJsonObject> TopoEmitter = MakeShared<FJsonObject>();
			TopoEmitter->SetStringField(TEXT("name"), EmitterName);
			TopoEmitter->SetBoolField(TEXT("enabled"), Topology.bEnabled);
			TArray<TSharedPtr<FJsonValue>> TopoMods;
			auto AppendStackMods = [&TopoMods](
				const FNiagaraExt_ScriptStackTopology& Stack,
				const TCHAR* ScriptUsage)
			{
				for (const FNiagaraExt_ModuleTopology& Mod : Stack.Modules)
				{
					TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
					M->SetStringField(TEXT("name"), Mod.ModuleName.ToString());
					M->SetStringField(TEXT("script_usage"), ScriptUsage);
					M->SetBoolField(TEXT("enabled"), Mod.Enabled);
					if (Mod.ModuleScript)
					{
						M->SetStringField(TEXT("module_script"), Mod.ModuleScript->GetPathName());
						M->SetStringField(
							TEXT("primitive_id"),
							Mod.ModuleScript->GetName());
					}
					M->SetBoolField(TEXT("is_set_parameters"), Mod.bIsSetParametersModule);
					TopoMods.Add(MakeShared<FJsonValueObject>(M));
				}
			};
			AppendStackMods(Topology.EmitterSpawnScript, TEXT("EmitterSpawnScript"));
			AppendStackMods(Topology.EmitterUpdateScript, TEXT("EmitterUpdateScript"));
			AppendStackMods(Topology.ParticleSpawnScript, TEXT("ParticleSpawnScript"));
			AppendStackMods(Topology.ParticleUpdateScript, TEXT("ParticleUpdateScript"));
			TopoEmitter->SetArrayField(TEXT("modules"), TopoMods);
			TArray<TSharedPtr<FJsonValue>> TopoRenderers;
			for (const TSubclassOf<UNiagaraRendererProperties>& Cls : Topology.RendererClasses)
			{
				if (Cls)
				{
					TopoRenderers.Add(MakeShared<FJsonValueString>(Cls->GetName()));
				}
			}
			TopoEmitter->SetArrayField(TEXT("renderers"), TopoRenderers);
			TopologyEmitterVals.Add(MakeShared<FJsonValueObject>(TopoEmitter));
		}

		// --- Emitter graph ---
		TSharedPtr<FJsonObject> EmitterGraph = MakeShared<FJsonObject>();
		EmitterGraph->SetStringField(TEXT("asset_path"), AssetPath);
		EmitterGraph->SetStringField(TEXT("graph_id"), EmitterGraphId);
		EmitterGraph->SetStringField(TEXT("graph_name"), EmitterName);
		EmitterGraph->SetStringField(TEXT("graph_type"), TEXT("NiagaraEmitterGraph"));
		EmitterGraph->SetStringField(TEXT("schema_version"), GGraphSchemaVersion);

		const bool bHasRenderers = Spec.bIncludeRenderers && Topology.Renderers.Num() > 0;
		EmitterGraph->SetObjectField(
			TEXT("fidelity"),
			FUeremcpNiagaraInspectMapping::MakeEmitterGraphFidelity(bHasRenderers));

		TArray<FString> StackSubgraphIds;
		static const FName StackNames[] = {
			TEXT("EmitterSpawnScript"),
			TEXT("EmitterUpdateScript"),
			TEXT("ParticleSpawnScript"),
			TEXT("ParticleUpdateScript"),
		};

		for (const FName& StackName : StackNames)
		{
			const FString StackStr = StackName.ToString();
			if (!ShouldIncludeStack(Spec, StackStr))
			{
				continue;
			}

			const FNiagaraExt_ScriptStackTopology* Stack = GetStackByName(Topology, StackName);
			if (!Stack)
			{
				continue;
			}

			const FString StackGraphId = FString::Printf(TEXT("%s::%s::%s"), *AssetPath, *EmitterName, *StackStr);
			StackSubgraphIds.Add(StackGraphId);

			TSharedPtr<FJsonObject> StackGraph = BuildModuleStackGraph(
				AssetPath,
				EmitterName,
				*Stack,
				InputValuesByModule,
				Spec.bIncludeInputValues,
				bOmitNodes,
				OutResult.ModuleCount);
			OutResult.Graphs.Add(MakeShared<FJsonValueObject>(StackGraph));
		}

		TArray<TSharedPtr<FJsonValue>> SubgraphValues;
		for (const FString& Id : StackSubgraphIds)
		{
			SubgraphValues.Add(MakeShared<FJsonValueString>(Id));
		}
		EmitterGraph->SetArrayField(TEXT("subgraphs"), SubgraphValues);

		TSharedPtr<FJsonObject> EmitterExtRoot = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> EmitterExt = MakeShared<FJsonObject>();
		EmitterExt->SetStringField(TEXT("emitter_name"), EmitterName);
		EmitterExt->SetBoolField(TEXT("bEnabled"), Topology.bEnabled);
		EmitterExt->SetStringField(TEXT("sim_target"), SimTargetToString(Topology.SimTarget));

		if (TSharedPtr<FJsonObject> LifeCycle =
			FUeremcpNiagaraEmitterProperties::ReadLifeCycleFromEmitterState(
				System, Context, EmitterName, OutResult.InternalOperations))
		{
			EmitterExt->SetObjectField(TEXT("life_cycle"), LifeCycle);
			OutResult.ChecksPerformed.Add(TEXT("niagara.emitter_life_cycle_read"));
		}

		if (Topology.RendererClasses.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> RendererClasses;
			for (const TSubclassOf<UNiagaraRendererProperties>& RendererClass : Topology.RendererClasses)
			{
				if (RendererClass)
				{
					RendererClasses.Add(MakeShared<FJsonValueString>(RendererClass->GetPathName()));
				}
			}
			EmitterExt->SetArrayField(TEXT("renderer_classes"), RendererClasses);
		}

		TArray<TSharedPtr<FJsonValue>> RendererNodes;
		if (Spec.bIncludeRenderers)
		{
			if (Topology.Renderers.Num() > 0)
			{
				bool bFetchedRendererData = false;
				const TArray<TSharedPtr<FJsonValue>> Renderers =
					FUeremcpNiagaraInspectMapping::BuildRendererExtensionEntries(
						System,
						EmitterSummary.EmitterName,
						Topology,
						Context,
						OutResult.InternalOperations,
						bFetchedRendererData);
				EmitterExt->SetArrayField(TEXT("renderers"), Renderers);
				OutResult.RendererCount += Topology.Renderers.Num();
				OutResult.ChecksPerformed.Add(TEXT("niagara.emitter_renderer_topology"));

				if (bFetchedRendererData)
				{
					OutResult.ChecksPerformed.Add(TEXT("niagara.renderer_data"));
				}

				if (!bOmitNodes)
				{
					RendererNodes = FUeremcpNiagaraInspectMapping::BuildRendererGraphNodes(EmitterName, Topology);
				}
			}
			else
			{
				EmitterExt->SetArrayField(TEXT("renderers"), TArray<TSharedPtr<FJsonValue>>());
			}
		}
		else
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.emitter_renderers"));
		}

		EmitterExtRoot->SetObjectField(TEXT("niagara"), EmitterExt);
		EmitterGraph->SetObjectField(TEXT("extensions"), EmitterExtRoot);

		if (!bOmitNodes && RendererNodes.Num() > 0)
		{
			EmitterGraph->SetArrayField(TEXT("nodes"), RendererNodes);
		}

		if (bHasRenderers)
		{
			TSharedPtr<FJsonObject> EmitterDiag = MakeShared<FJsonObject>();
			EmitterDiag->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>{
				MakeShared<FJsonValueString>(
					TEXT("renderer material_path values are best-effort extracts from renderer observability fields and are not validated (renderer_material_bindings)"))
			});
			EmitterGraph->SetObjectField(TEXT("diagnostics"), EmitterDiag);
		}

		OutResult.Graphs.Add(MakeShared<FJsonValueObject>(EmitterGraph));
		OutResult.EmitterNames.Add(EmitterName);
		++OutResult.EmitterCount;

		TSharedPtr<FJsonObject> EmitterNode = MakeShared<FJsonObject>();
		EmitterNode->SetStringField(TEXT("node_id"), FString::Printf(TEXT("emitter_%s"), *EmitterName));
		EmitterNode->SetStringField(TEXT("semantic_id"), EmitterName);
		EmitterNode->SetStringField(TEXT("node_class"), TEXT("FNiagaraEmitterHandle"));
		EmitterNode->SetStringField(TEXT("semantic_type"), TEXT("niagara_emitter"));
		EmitterNode->SetStringField(TEXT("title"), EmitterName);
		EmitterNode->SetBoolField(TEXT("enabled"), EmitterSummary.bEnabled);
		SystemNodes.Add(MakeShared<FJsonValueObject>(EmitterNode));
	}

	TArray<TSharedPtr<FJsonValue>> SystemSubgraphs;
	for (const FString& Id : EmitterSubgraphIds)
	{
		SystemSubgraphs.Add(MakeShared<FJsonValueString>(Id));
	}
	SystemGraph->SetArrayField(TEXT("subgraphs"), SystemSubgraphs);
	if (!bOmitNodes)
	{
		SystemGraph->SetArrayField(TEXT("nodes"), SystemNodes);
	}

	TSharedPtr<FJsonObject> SystemExtRoot = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> SystemExt = MakeShared<FJsonObject>();
	SystemExt->SetStringField(TEXT("system_name"), Summary.SystemName.ToString());

	TArray<TSharedPtr<FJsonValue>> UserParams;
	for (const FNiagaraExt_UserVariable& Var : Summary.UserVariables)
	{
		TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
		Param->SetStringField(TEXT("name"), Var.Name.ToString());
		UserParams.Add(MakeShared<FJsonValueObject>(Param));
	}
	SystemExt->SetArrayField(TEXT("user_parameters"), UserParams);

	if (Spec.bIncludeDependencies)
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.system_dependencies"));

		TSharedPtr<FJsonObject> Deps = MakeShared<FJsonObject>();
		Deps->SetNumberField(TEXT("used_modules"), DependencySurvey.UsedModules);
		Deps->SetNumberField(TEXT("used_data_interfaces"), DependencySurvey.UsedDataInterfaces);
		Deps->SetNumberField(TEXT("used_dynamic_inputs"), DependencySurvey.UsedDynamicInputs);
		Deps->SetNumberField(TEXT("used_renderers"), DependencySurvey.UsedRenderers);
		Deps->SetStringField(
			TEXT("survey_fidelity"),
			TEXT("topology_and_script_default_dis_only; live stack DI inputs not serialized"));
		SystemExt->SetObjectField(TEXT("dependencies"), Deps);
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.system_dependencies"));
	}

	FNiagaraExt_SystemCompileState CompileState;
	bool bHasCompileState = false;
	if (Spec.bIncludeCompileState)
	{
		UNiagaraExternalEditUtilities::GetSystemCompileState(System, CompileState, Context);
		++OutResult.InternalOperations;
		OutResult.ChecksPerformed.Add(TEXT("niagara.compile_state"));
		bHasCompileState = true;

		TSharedPtr<FJsonObject> Compile = MakeShared<FJsonObject>();
		Compile->SetStringField(TEXT("aggregate_status"), CompileStatusToString(CompileState.AggregateStatus));
		Compile->SetBoolField(TEXT("bIsCompiling"), CompileState.bIsCompiling);
		Compile->SetBoolField(TEXT("bHasErrors"), CompileState.bHasErrors);

		TArray<TSharedPtr<FJsonValue>> PerScript;
		for (const FNiagaraExt_ScriptCompileInfo& ScriptInfo : CompileState.Scripts)
		{
			TSharedPtr<FJsonObject> ScriptObj = MakeShared<FJsonObject>();
			ScriptObj->SetStringField(TEXT("emitter_name"), ScriptInfo.EmitterName.ToString());
			ScriptObj->SetStringField(TEXT("script_usage"), ScriptInfo.ScriptName.ToString());
			ScriptObj->SetStringField(TEXT("status"), CompileStatusToString(ScriptInfo.LastCompileStatus));
			PerScript.Add(MakeShared<FJsonValueObject>(ScriptObj));
		}
		Compile->SetArrayField(TEXT("per_script"), PerScript);

		SystemExt->SetObjectField(TEXT("compile"), Compile);

		const bool bUpToDate = CompileState.AggregateStatus == ENiagaraExt_ScriptCompileStatus::UpToDate;
		OutResult.bCompiled = !CompileState.bIsCompiling && !CompileState.bHasErrors && bUpToDate;
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.compile_state"));
	}

	FNiagaraExt_StackIssues StackIssues;
	bool bHasStackIssues = false;
	if (Spec.bIncludeStackIssues)
	{
		if (ShouldSkipStackIssuesForProbe(AssetPath))
		{
			OutResult.ChecksSkipped.Add(TEXT("niagara.stack_issues"));
			OutResult.ChecksSkipped.Add(TEXT(
				"niagara.stack_issues_skipped_probe: GetStackIssues diagnostics VM evaluates mesh OverrideMaterials edit conditions"));
		}
		else
		{
			UNiagaraExternalEditUtilities::GetStackIssues(System, StackIssues, Context);
			++OutResult.InternalOperations;
			OutResult.ChecksPerformed.Add(TEXT("niagara.stack_issues"));
			bHasStackIssues = true;
			SystemExt->SetNumberField(TEXT("stack_issue_count"), StackIssues.Issues.Num());
		}
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.stack_issues"));
	}

	{
		TArray<TSharedPtr<FJsonValue>> EventHandlers =
			FUeremcpNiagaraInspectMapping::BuildEventHandlersFromEmitterData(
				System, OutResult.InternalOperations);
		OutResult.ChecksPerformed.Add(TEXT("niagara.event_handlers_from_GetEventHandlers"));

		if (bHasCompileState || bHasStackIssues)
		{
			FNiagaraExt_SystemCompileState EmptyCompile;
			FNiagaraExt_StackIssues EmptyIssues;
			const TArray<TSharedPtr<FJsonValue>> Inferred =
				FUeremcpNiagaraInspectMapping::BuildEventHandlerPlaceholders(
					bHasStackIssues ? StackIssues : EmptyIssues,
					bHasCompileState ? CompileState : EmptyCompile);
			// Merge inferred-only entries (systems where GetEventHandlers empty but issues mention handlers).
			TSet<FString> Seen;
			for (const TSharedPtr<FJsonValue>& H : EventHandlers)
			{
				const TSharedPtr<FJsonObject> Obj = H.IsValid() ? H->AsObject() : nullptr;
				if (!Obj.IsValid())
				{
					continue;
				}
				FString Emitter;
				FString Event;
				Obj->TryGetStringField(TEXT("source_emitter"), Emitter);
				Obj->TryGetStringField(TEXT("event_name"), Event);
				Seen.Add(Emitter + TEXT("|") + Event);
			}
			for (const TSharedPtr<FJsonValue>& H : Inferred)
			{
				const TSharedPtr<FJsonObject> Obj = H.IsValid() ? H->AsObject() : nullptr;
				if (!Obj.IsValid())
				{
					continue;
				}
				FString Emitter;
				FString Event;
				Obj->TryGetStringField(TEXT("source_emitter"), Emitter);
				Obj->TryGetStringField(TEXT("event_name"), Event);
				const FString Key = Emitter + TEXT("|") + Event;
				if (!Seen.Contains(Key))
				{
					Seen.Add(Key);
					EventHandlers.Add(H);
				}
			}
		}

		SystemExt->SetArrayField(TEXT("event_handlers"), EventHandlers);
		if (EventHandlers.Num() > 0)
		{
			SystemExt->SetStringField(
				TEXT("event_handlers_fidelity"),
				TEXT("GetEventHandlers metadata + NodeGraph module samples; "
					 "write blocked (no UsageId on StackItemReference)"));
		}
	}

	SystemExtRoot->SetObjectField(TEXT("niagara"), SystemExt);
	SystemGraph->SetObjectField(TEXT("extensions"), SystemExtRoot);

	TSharedPtr<FJsonObject> SysDiag = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Warnings;
	Warnings.Add(MakeShared<FJsonValueString>(
		TEXT("event_handlers: READ via GetEventHandlers + NodeGraph samples. "
			 "WRITE AddModule/enable on ParticleEventScript blocked — "
			 "FNiagaraExt_StackItemReference has no UsageId (FindScriptGroup needs Guid). "
			 "script_graph_internals: READ summary only; no EdGraph mutate API.")));
	SysDiag->SetArrayField(TEXT("warnings"), Warnings);
	SystemGraph->SetObjectField(TEXT("diagnostics"), SysDiag);

	OutResult.Graphs.Insert(MakeShared<FJsonValueObject>(SystemGraph), 0);

	const int32 HashedGraphs = FUeremcpNiagaraGraphHash::ApplyContentHashesToGraphs(
		OutResult.Graphs,
		OutResult.ChecksPerformed,
		OutResult.ChecksSkipped);

	AddTrace(
		OutResult.ExecutionTrace,
		TEXT("map_topology"),
		true,
		FString::Printf(TEXT("%d emitters, %d module nodes"), OutResult.EmitterCount, OutResult.ModuleCount));

	if (OutResult.TopologySummary.IsValid())
	{
		OutResult.TopologySummary->SetArrayField(TEXT("emitters"), TopologyEmitterVals);
		OutResult.TopologySummary->SetNumberField(TEXT("emitter_count"), OutResult.EmitterCount);
		OutResult.TopologySummary->SetNumberField(TEXT("module_count"), OutResult.ModuleCount);
		OutResult.TopologySummary->SetNumberField(TEXT("renderer_count"), OutResult.RendererCount);
		OutResult.TopologySummary->SetBoolField(TEXT("round_trip_supported"), false);
		OutResult.ChecksPerformed.Add(TEXT("niagara.topology_summary"));
	}

	OutResult.Summary = FString::Printf(
		TEXT("Inspected Niagara system '%s': %d emitter graph(s), %d module stack node(s), %d renderer ref(s), %d content_hash(es) via UNiagaraExternalEditUtilities + FUeremcpContentHash. "
			 "result.topology_summary lists emitters→modules→renderers at top. "
			 "event_handlers from GetEventHandlers (write blocked). "
			 "sim_target + life_cycle readable. round_trip_supported=false until retrieve→submit→retrieve proven."),
		*Summary.SystemName.ToString(),
		OutResult.EmitterCount,
		OutResult.ModuleCount,
		OutResult.RendererCount,
		HashedGraphs);

	OutResult.bSuccess = true;
	OutResult.ChecksPerformed.Add(TEXT("niagara.topology_read"));
	return true;
}
