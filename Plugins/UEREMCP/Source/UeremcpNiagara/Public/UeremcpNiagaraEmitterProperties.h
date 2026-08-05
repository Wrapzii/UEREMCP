// UEREMCP — Emitter Properties (SimTarget) + Emitter State Life Cycle write/read.
//
// SimTarget → SetEmitterData PropertyValues (FVersionedNiagaraEmitterData.SimTarget).
// [VERIFIED: SetEmitterData — NiagaraExternalSystemEditorUtilities.h:1375]
// [VERIFIED: FVersionedNiagaraEmitterData::SimTarget — NiagaraEmitter.h:336]
//
// Life Cycle (Loop Duration / Loop Behavior / Life Cycle Mode / Inactive Response)
// live on Emitter State module inputs (Details panel), NOT on FVersionedNiagaraEmitterData.
// [VERIFIED: NiagaraConvertInPlaceEmitterAndSystemState.cpp:80-135 input names]
// Applied via SetStackInputData on EmitterUpdateScript / EmitterState.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NiagaraExternalSystemEditorUtilities.h"

/** Parsed first-class emitter properties matching Sparks Details panel fields. */
struct FUeremcpNiagaraEmitterPropertyPlan
{
	/** CPUSim | GPUComputeSim (empty = leave unchanged). */
	FString SimTarget;

	bool bHasEnabled = false;
	bool bEnabled = true;

	/** Life Cycle Mode enum name (e.g. Self / System) — Emitter State input. */
	FString LifeCycleMode;

	/** Loop Behavior enum name (Once / Multiple / Infinite). */
	FString LoopBehavior;

	/** Loop Duration seconds (Emitter State "Loop Duration"). */
	TOptional<float> LoopDuration;

	/** Inactive Response enum name. */
	FString InactiveResponse;

	bool HasLifeCycleFields() const
	{
		return !LifeCycleMode.IsEmpty()
			|| !LoopBehavior.IsEmpty()
			|| LoopDuration.IsSet()
			|| !InactiveResponse.IsEmpty();
	}

	bool HasAny() const
	{
		return !SimTarget.IsEmpty() || bHasEnabled || HasLifeCycleFields();
	}
};

class FUeremcpNiagaraEmitterProperties
{
public:
	/** Parse sim_target / life_cycle / enabled from an emitter JSON object or extensions.niagara. */
	static void ParseFromJsonObject(
		const TSharedPtr<FJsonObject>& Obj,
		FUeremcpNiagaraEmitterPropertyPlan& InOutPlan);

	/** Normalize sim_target aliases → CPUSim | GPUComputeSim. Empty if unrecognized. */
	static FString NormalizeSimTarget(const FString& Raw);

	/**
	 * Write SimTarget (+ optional bIsEnabled) via GetEmitterData / SetEmitterData.
	 * [VERIFIED: SetEmitterHandleAndDataProperties merges SimTarget into FVersionedNiagaraEmitterData]
	 */
	static bool ApplySimTargetAndEnabled(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		const FUeremcpNiagaraEmitterPropertyPlan& Plan,
		int32& InOutOps,
		TArray<FString>& OutApplied,
		TArray<FString>& OutWarnings);

	/**
	 * Write Life Cycle fields onto Emitter State module inputs (EmitterUpdateScript).
	 * Finds EmitterState / Emitter State module; creates none if missing (warning).
	 */
	static bool ApplyLifeCycleViaEmitterState(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		const FUeremcpNiagaraEmitterPropertyPlan& Plan,
		int32& InOutOps,
		TArray<FString>& OutApplied,
		TArray<FString>& OutWarnings);

	/** Apply all present fields (SimTarget then Life Cycle). */
	static void ApplyAll(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		const FUeremcpNiagaraEmitterPropertyPlan& Plan,
		int32& InOutOps,
		TArray<FString>& OutApplied,
		TArray<FString>& OutWarnings);

	/**
	 * Read life_cycle object from Emitter State module inputs for inspect extensions.
	 * Returns null if Emitter State not found / inputs unavailable.
	 */
	static TSharedPtr<FJsonObject> ReadLifeCycleFromEmitterState(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		int32& InOutOps);
};
