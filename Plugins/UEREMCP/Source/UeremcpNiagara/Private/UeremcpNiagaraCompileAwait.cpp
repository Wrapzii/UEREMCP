// UEREMCP — compile await without QueryCompileComplete (WS-07).

#include "UeremcpNiagaraCompileAwait.h"

#include "NiagaraSystem.h"

#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Containers/Ticker.h"
#include "AssetCompilingManager.h"
#include "HAL/PlatformProcess.h"

namespace
{
	void PumpNiagaraCompileWait(bool bLimitExecutionTime)
	{
		// [VERIFIED: Engine/Source/Runtime/Core/Public/Containers/Ticker.h:81]
		FTSTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
#if WITH_EDITOR
		// [VERIFIED: Engine/Source/Runtime/Engine/Public/AssetCompilingManager.h:97]
		FAssetCompilingManager::Get().ProcessAsyncTasks(bLimitExecutionTime);
#endif
	}

	bool IsLiveToolDispatchContext()
	{
		return !GIsAutomationTesting;
	}
}

bool FUeremcpNiagaraCompileAwait::IsAggregateCompileUpToDate(ENiagaraExt_ScriptCompileStatus Status)
{
	return Status == ENiagaraExt_ScriptCompileStatus::UpToDate
		|| Status == ENiagaraExt_ScriptCompileStatus::UpToDateWithWarnings
		|| Status == ENiagaraExt_ScriptCompileStatus::ComputeUpToDateWithWarnings;
}

bool FUeremcpNiagaraCompileAwait::IsScriptDerivedCompileComplete(
	const FNiagaraExt_SystemCompileState& State)
{
	if (State.bHasErrors || State.Scripts.Num() == 0)
	{
		return false;
	}

	for (const FNiagaraExt_ScriptCompileInfo& Script : State.Scripts)
	{
		if (Script.LastCompileStatus == ENiagaraExt_ScriptCompileStatus::Dirty
			|| Script.LastCompileStatus == ENiagaraExt_ScriptCompileStatus::BeingCreated
			|| Script.LastCompileStatus == ENiagaraExt_ScriptCompileStatus::Error)
		{
			return false;
		}
	}

	return IsAggregateCompileUpToDate(State.AggregateStatus);
}

FUeremcpNiagaraCompileAwaitResult FUeremcpNiagaraCompileAwait::AwaitCompile(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	int32 TimeoutSeconds,
	FNiagaraExt_SystemCompileState& OutState)
{
	FUeremcpNiagaraCompileAwaitResult Result;

	if (!System || !IsInGameThread())
	{
		return Result;
	}

	// Stack input edits can leave the cached runtime SystemStateData unchanged when
	// Niagara considers the scripts otherwise up to date. A forced compile runs the
	// post-compile cache refresh before the generated package is saved.
	// [VERIFIED: NiagaraSystem.cpp:3608-3621]
	System->RequestCompile(true);

	const double Deadline = FPlatformTime::Seconds() + static_cast<double>(TimeoutSeconds);
	while (FPlatformTime::Seconds() < Deadline)
	{
		UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);

		const bool bScriptStateComplete = IsScriptDerivedCompileComplete(OutState);
		if (bScriptStateComplete
			&& (IsLiveToolDispatchContext() || !OutState.bIsCompiling))
		{
			Result.bAwaited = true;
			if (IsLiveToolDispatchContext())
			{
				Result.bObservedViaScriptState = true;
				if (OutState.bIsCompiling)
				{
					Result.bActiveQueueNotDrained = true;
					OutState.bIsCompiling = false;
				}
			}
			return Result;
		}

		PumpNiagaraCompileWait(/*bLimitExecutionTime=*/false);

		if (!IsLiveToolDispatchContext())
		{
			// Editor automation must keep polling until the script-derived state catches
			// up. One poll is insufficient after multiple stack-input edits: the first
			// pass can only promote the queued request into an active compile.
			if (System->HasActiveCompilations()
				|| System->HasOutstandingCompilationRequests(/*bIncludingGPUShaders=*/false))
			{
				System->PollForCompilationComplete(/*bFlushRequestCompile=*/false);
			}
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		FPlatformProcess::Sleep(0.01f);
	}

	UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);

	if (IsScriptDerivedCompileComplete(OutState)
		&& (IsLiveToolDispatchContext() || !OutState.bIsCompiling))
	{
		Result.bAwaited = true;
		if (IsLiveToolDispatchContext())
		{
			Result.bObservedViaScriptState = true;
			if (OutState.bIsCompiling)
			{
				Result.bActiveQueueNotDrained = true;
				OutState.bIsCompiling = false;
			}
		}
		return Result;
	}

	Result.bAwaited = !OutState.bIsCompiling;
	return Result;
}
