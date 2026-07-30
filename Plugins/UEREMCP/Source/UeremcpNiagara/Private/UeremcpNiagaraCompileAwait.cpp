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

	System->RequestCompile(false);

	const double Deadline = FPlatformTime::Seconds() + static_cast<double>(TimeoutSeconds);
	while (FPlatformTime::Seconds() < Deadline)
	{
		UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);

		if (IsScriptDerivedCompileComplete(OutState))
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
			// Editor automation: one bounded PollForCompilationComplete pass (uses QueryCompileComplete).
			if (!System->HasActiveCompilations()
				&& !System->HasOutstandingCompilationRequests(/*bIncludingGPUShaders=*/false))
			{
				break;
			}
			System->PollForCompilationComplete(/*bFlushRequestCompile=*/false);
			break;
		}

		FPlatformProcess::Sleep(0.01f);
	}

	UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);

	if (IsScriptDerivedCompileComplete(OutState))
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
