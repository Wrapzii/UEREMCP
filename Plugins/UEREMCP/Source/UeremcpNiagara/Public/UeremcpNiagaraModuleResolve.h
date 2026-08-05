// UEREMCP — resolve module short names / script stacks for LLM-authored emitters (WS-07).
//
// Epic requires an emitter template for AddEmitter; custom stacks use the Minimal
// substrate then UNiagaraExternalEditUtilities::AddModule per JSON.
// [VERIFIED: NiagaraExternalSystemEditorUtilities.h AddEmitter / AddModule]
// [VERIFIED: DefaultNiagara.ini DefaultEmptyEmitter=/Niagara/.../Minimal.Minimal]

#pragma once

#include "CoreMinimal.h"

class UNiagaraScript;
class UNiagaraRendererProperties;

namespace UeremcpNiagaraModuleResolve
{
	/** Soft path for Epic's empty/Minimal emitter template (AddEmitter substrate). */
	inline const TCHAR* MinimalEmitterTemplatePath()
	{
		return TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal");
	}

	/**
	 * Resolve a module asset soft path from primitive_id / name and/or explicit asset_path.
	 * Known short names + primitive_ids map to /Niagara/Modules/... scripts; unknown
	 * names fail honestly (custom HLSL / script-graph authorship unsupported).
	 */
	bool ResolveModuleAssetPath(
		const FString& NameOrAlias,
		const FString& ExplicitAssetPath,
		FString& OutPath,
		FString& OutError);

	/** Alias: primitive_id (e.g. spawn_rate) → same catalog as ResolveModuleAssetPath. */
	bool ResolvePrimitiveId(
		const FString& PrimitiveId,
		FString& OutPath,
		FString& OutDisplayName,
		FString& OutError);

	/** Load UNiagaraScript from a soft path (package or package.asset). */
	UNiagaraScript* LoadModuleScript(const FString& SoftPath, FString& OutError);

	/**
	 * Default script stack for a module when the agent omits script/script_usage.
	 * Returns EmitterUpdateScript / ParticleSpawnScript / ParticleUpdateScript.
	 */
	FString DefaultScriptUsageForModule(const FString& ModuleNameOrPath);

	/** Normalize script usage aliases (emitter_update → EmitterUpdateScript, etc.). */
	FString NormalizeScriptUsage(const FString& InUsage);

	/**
	 * Map renderer hint (sprite|mesh|ribbon|light|…) to a Niagara renderer class.
	 * Returns nullptr when hint is empty or unknown (caller skips AddRenderer).
	 */
	TSubclassOf<UNiagaraRendererProperties> ResolveRendererClass(
		const FString& RendererTypeHint,
		FString& OutError);
}
