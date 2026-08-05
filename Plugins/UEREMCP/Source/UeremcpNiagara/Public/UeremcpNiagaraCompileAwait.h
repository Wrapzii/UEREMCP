// UEREMCP — compile await without QueryCompileComplete (WS-07 / MCP B1 B6).
//
// MCP synchronous tool dispatch must not call PollForCompilationComplete /
// QueryCompileComplete (hybrid ActiveCompilations crash). Observe completion via
// UNiagaraExternalEditUtilities::GetSystemCompileState script VM statuses instead.
//
// Failure mode (editor assert): FTSTicker / FAssetCompilingManager::ProcessAsyncTasks
// during live await can reach INiagaraModule::PollSystemCompilations →
// UNiagaraSystem::QueryCompileComplete → TSharedPtr::operator-> check(IsValid())
// on a hybrid/stale ActiveCompilations task.
// [VERIFIED: NiagaraModule.cpp:574-586]
// [VERIFIED: NiagaraSystem.cpp:3532-3548]
// [VERIFIED: Engine/Source/Runtime/Core/Public/Templates/SharedPointer.h:1131-1134]

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
	/**
	 * True when live MCP/toolset dispatch skipped FTSTicker + ProcessAsyncTasks to avoid
	 * PollSystemCompilations → QueryCompileComplete SharedPtr assert.
	 */
	bool bLiveEnginePumpSkipped = false;
	/** Non-empty when AwaitCompile must surface a structured envelope failure (never assert). */
	FString Error;
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
	 * Live path also skips ticker/AssetCompilingManager pumps (see header failure mode).
	 * On precondition failure, returns Error set and bAwaited=false — callers must fail the envelope.
	 */
	static FUeremcpNiagaraCompileAwaitResult AwaitCompile(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		int32 TimeoutSeconds,
		FNiagaraExt_SystemCompileState& OutState);
};
