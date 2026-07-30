// UEREMCP — Inspect graph mapping helpers (WS-07).
//
// Pure mapping logic shared by inspect and round-trip validation tests.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FNiagaraExt_EmitterTopology;
struct FNiagaraExt_RendererData;
struct FNiagaraExt_ScriptCompileInfo;
struct FNiagaraExt_StackIssue;
struct FNiagaraExt_StackIssues;
struct FNiagaraExt_SystemCompileState;
struct FNiagaraExternalEditContext;
class UNiagaraSystem;

/** Lossy event-handler placeholders — not from GetEmitterTopology. */
class FUeremcpNiagaraInspectMapping
{
public:
	/** Fidelity block for NiagaraEmitterGraph; adds renderer_material_bindings when renderers exist. */
	static TSharedPtr<FJsonObject> MakeEmitterGraphFidelity(bool bHasRenderers);

	/**
	 * Map GetEmitterTopology.Renderers + GetRendererData into extensions.niagara.renderers[].
	 * [VERIFIED: GetEmitterTopology — NiagaraExternalSystemEditorUtilities.h:867-905]
	 * [VERIFIED: GetRendererData — NiagaraExternalSystemEditorUtilities.h:1311]
	 */
	static TArray<TSharedPtr<FJsonValue>> BuildRendererExtensionEntries(
		UNiagaraSystem* System,
		const FName& EmitterName,
		const FNiagaraExt_EmitterTopology& Topology,
		FNiagaraExternalEditContext& Context,
		int32& InOutInternalOperations,
		bool& bOutFetchedPropertyValues);

	/** Summary renderer nodes for NiagaraEmitterGraph.nodes[] (topology refs only). */
	static TArray<TSharedPtr<FJsonValue>> BuildRendererGraphNodes(
		const FString& EmitterName,
		const FNiagaraExt_EmitterTopology& Topology);

	/** Best-effort material soft path from GetRendererData PropertyValues JSON blob. */
	static FString TryExtractMaterialPath(const FString& PropertyValuesJson);
	/**
	 * Infer event-handler entries from GetStackIssues + compile script list.
	 * GetEmitterTopology omits ParticleEventScript stacks
	 * [VERIFIED: RB-07 / NiagaraExternalSystemEditorUtilities usage gap].
	 */
	static TArray<TSharedPtr<FJsonValue>> BuildEventHandlerPlaceholders(
		const FNiagaraExt_StackIssues& Issues,
		const FNiagaraExt_SystemCompileState& CompileState);

	/** Locate the NiagaraSystemGraph object inside an inspect graphs array. */
	static TSharedPtr<FJsonObject> FindSystemGraph(const TArray<TSharedPtr<FJsonValue>>& Graphs);

	/** Count NiagaraEmitterGraph entries. */
	static int32 CountEmitterGraphs(const TArray<TSharedPtr<FJsonValue>>& Graphs);

	/** Read user parameter names from system graph extensions.niagara.user_parameters. */
	static TArray<FString> ReadUserParameterNames(const TSharedPtr<FJsonObject>& SystemGraph);
};
