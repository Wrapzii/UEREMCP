// UEREMCP — goal-level Niagara effect creation (WS-07 / POC B slice).
//
// Composes UNiagaraExternalEditUtilities (public NiagaraEditor API) — same substrate
// as Epic NiagaraToolsets.NiagaraToolset_System.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"
#include "UeremcpNiagaraMaterialBinding.h"

/** One module to AddModule onto an emitter script stack (declarative primitive). */
struct FUeremcpNiagaraModulePlan
{
	/** Catalog id (spawn_rate, initialize_particle, …) — preferred LLM handle. */
	FString PrimitiveId;

	/** Display / stack name hint (also used for short-name catalog lookup). */
	FString Name;

	/** Explicit UNiagaraScript soft path (module_script / asset_path). */
	FString AssetPath;

	/** EmitterSpawnScript | EmitterUpdateScript | ParticleSpawnScript | ParticleUpdateScript. */
	FString ScriptUsage;

	bool bEnabled = true;

	/**
	 * Stack inputs as JSON object (name → number|bool|[r,g,b,a] |
	 * {mode:linked|hlsl_expression|data_interface|dynamic_input|enum|local}).
	 * Applied via SetStackInputData after AddModule.
	 * [VERIFIED: FNiagaraExt_StackInputData_* — NiagaraExternalSystemEditorUtilities.h:520-594]
	 */
	TSharedPtr<FJsonObject> Inputs;
};

/** One emitter to add — LLM-authored modules[] and/or optional role/template shortcut. */
struct FUeremcpNiagaraEmitterPlan
{
	/** Final emitter name on the system (e.g. GroundMist). */
	FString Name;

	/** Optional semantic role shortcut (sparks, circle, …) — NOT required when modules[] set. */
	FString Role;

	/**
	 * Emitter template soft path for AddEmitter clone.
	 * When modules[] drives the stack and no role/template given, resolved to Minimal.
	 * [VERIFIED: DefaultNiagara.ini DefaultEmptyEmitter → Minimal]
	 */
	FString TemplatePath;

	/** LLM-defined module stack to AddModule after cloning the substrate. */
	TArray<FUeremcpNiagaraModulePlan> Modules;

	/** Optional renderer hint: sprite | mesh | ribbon | light. */
	FString RendererType;

	bool bEnabled = true;
	bool bHasEnabled = false;

	/**
	 * First-class Emitter Properties + Emitter State Life Cycle.
	 * SimTarget → SetEmitterData; Loop Duration / Life Cycle Mode / Loop Behavior /
	 * Inactive Response → Emitter State SetStackInputData.
	 */
	FString SimTarget;
	FString LifeCycleMode;
	FString LoopBehavior;
	TOptional<float> LoopDuration;
	FString InactiveResponse;

	/** True when modules[] (not a preset role kit) defines the stack. */
	bool bCustomModuleStack = false;
};

/** Parsed create_niagara_effect specification (schemas/domains/niagara/create_niagara_effect.schema.json). */
struct FUeremcpNiagaraCreateSpec
{
	FString Name;
	FString EffectType;
	FString Element;

	/** Emitter roles to instantiate (legacy shorthand; also filled from emitters[]). */
	TArray<FString> ComponentRoles;

	/**
	 * Resolved emitter plans (name + optional role/template + modules[]).
	 * Create::Run always adds these via UNiagaraExternalEditUtilities::AddEmitter,
	 * then AddModule for each modules[] entry.
	 */
	TArray<FUeremcpNiagaraEmitterPlan> Emitters;

	/** Optional template system soft path (package/object). */
	FString TemplateSystemPath;

	/** Optional existing Niagara system whose emitter structure is inherited by a variation. */
	FString BaseSystemPath;

	/** User-parameter knobs from specification.parameters. */
	TSharedPtr<FJsonObject> Parameters;

	/** specification.materials entries (direct path or create_spec placeholder). */
	TArray<FUeremcpNiagaraMaterialRequest> MaterialRequests;
};

/** Result of a create/modify Niagara compose pass. */
struct FUeremcpNiagaraCreateResult
{
	bool bSuccess = false;
	FString Error;

	FString Summary;
	FString CreatedAssetPath;
	FString InheritedAssetPath;

	TArray<FString> EmittersAdded;
	TArray<FString> EmittersInherited;
	TArray<FString> ModulesAdded;
	TArray<FString> RenderersAdded;
	TArray<FString> UserVariablesAdded;
	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	TArray<FString> LossyWarnings;

	bool bReplacedExisting = false;
	bool bMaterialBindingPartialFailure = false;

	FUeremcpNiagaraMaterialBindingResult MaterialBindings;

	TOptional<bool> bCompiled;
	TOptional<bool> bSaved;

	int32 InternalOperations = 0;

	/** Phase timings in milliseconds (schemas/common/defs.schema.json#/$defs/metrics.timing_ms). */
	TMap<FString, double> TimingMs;
};

/**
 * Duplicate-and-modify composer for Niagara systems under sandbox or Magecraft
 * (/Game/__UeremcpTests, /Game/__UeremcpPoc, /Game/RE/VFX/Magecraft).
 */
class FUeremcpNiagaraCreate
{
public:
	static bool ParseSpecification(
		const FUeremcpRequest& Request,
		FUeremcpNiagaraCreateSpec& OutSpec,
		FString& OutError);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraCreateSpec& Spec,
		FUeremcpNiagaraCreateResult& OutResult);
};
