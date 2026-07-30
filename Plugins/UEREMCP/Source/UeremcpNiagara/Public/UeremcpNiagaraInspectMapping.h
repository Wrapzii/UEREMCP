// UEREMCP — Inspect graph mapping helpers (WS-07).
//
// Pure mapping logic shared by inspect and round-trip validation tests.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FNiagaraExt_ScriptCompileInfo;
struct FNiagaraExt_StackIssue;
struct FNiagaraExt_StackIssues;
struct FNiagaraExt_SystemCompileState;

/** Lossy event-handler placeholders — not from GetEmitterTopology. */
class FUeremcpNiagaraInspectMapping
{
public:
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
