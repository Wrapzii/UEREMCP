// UEREMCP — compile await without QueryCompileComplete (WS-07 / MCP B1 B6).
//
// MCP synchronous tool dispatch must not call PollForCompilationComplete /
// QueryCompileComplete (hybrid ActiveCompilations crash). Observe completion via
// UNiagaraExternalEditUtilities::GetSystemCompileState script VM statuses instead.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraExternalSystemEditorUtilities.h"

struct FUeremcpNiagaraCompileAwaitResult
{
	bool bAwaited = false;
	/** True when VM script statuses are UpToDate but HasActiveCompilations queue was not drained. */
	bool bActiveQueueNotDrained = false;
	/** True when live tool dispatch used script-state observe poll (not QueryCompileComplete). */
	bool bObservedViaScriptState = false;
};

class FUeremcpNiagaraCompileAwait
{
public:
	static bool IsAggregateCompileUpToDate(ENiagaraExt_ScriptCompileStatus Status);

	/** Script VM LastCompileStatus aggregate — safe to read without QueryCompileComplete. */
	static bool IsScriptDerivedCompileComplete(const FNiagaraExt_SystemCompileState& State);

	/**
	 * RequestCompile then wait for script-derived UpToDate (MCP) or bounded poll (automation).
	 * Never calls QueryCompileComplete on live MCP/toolset dispatch.
	 */
	static FUeremcpNiagaraCompileAwaitResult AwaitCompile(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		int32 TimeoutSeconds,
		FNiagaraExt_SystemCompileState& OutState);
};
