// UEREMCP — submit edited Niagara graph JSON (WS-07).
//
// Agent-facing WRITE for inspect_system graphs[]. In-place apply under sandbox or
// Magecraft. Never deletes Magecraft UAssets. mode=replace may remove modules;
// destructive dry_run defaults per ADR-0010. round_trip_supported stays false
// until retrieve→replace→retrieve content_hash is proven.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

/** Parsed submit_niagara_graph specification. */
struct FUeremcpNiagaraSubmitSpec
{
	TArray<TSharedPtr<FJsonObject>> Graphs;

	bool bApplyModules = true;
	bool bApplyUserParameters = true;
	bool bApplyRendererMaterials = true;
	bool bApplyEmitterEnabled = true;
	/** Apply sim_target + life_cycle (Emitter State) from emitter graphs / emitters[]. */
	bool bApplyEmitterProperties = true;
	/** Add missing emitters when NiagaraEmitterGraph has role/template_path (default true). */
	bool bApplyAddEmitters = true;
};

struct FUeremcpNiagaraSubmitResult
{
	bool bSuccess = false;
	FString Error;

	FString Summary;
	FString AssetPath;

	TArray<FString> PlannedChanges;
	TArray<FString> ModulesEnabledChanged;
	TArray<FString> ModulesAdded;
	TArray<FString> ModulesRemoved;
	TArray<FString> UserVariablesTouched;
	TArray<FString> RendererMaterialsApplied;
	TArray<FString> EmittersAdded;
	TArray<FString> EmittersEnabledChanged;
	TArray<FString> EmitterPropertiesApplied;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	TArray<FString> LossyWarnings;

	TOptional<bool> bCompiled;
	TOptional<bool> bSaved;
	bool bStructuralMatchAfter = false;

	TArray<TSharedPtr<FJsonValue>> PostInspectGraphs;

	int32 InternalOperations = 0;
};

/** Apply edited ADR-0004 Niagara graphs to an existing system. */
class FUeremcpNiagaraSubmit
{
public:
	static bool ParseSpecification(
		const TSharedPtr<FJsonObject>& Specification,
		FUeremcpNiagaraSubmitSpec& OutSpec,
		FString& OutError);

	/**
	 * Apply graphs to target.asset_path.
	 * mode=replace: may remove modules not present in desired stacks.
	 * Magecraft: never deletes the asset package.
	 */
	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraSubmitSpec& Spec,
		FUeremcpNiagaraSubmitResult& OutResult);
};
