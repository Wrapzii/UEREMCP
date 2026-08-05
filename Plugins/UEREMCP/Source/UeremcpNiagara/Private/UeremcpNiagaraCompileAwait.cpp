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
		// Automation-only. Live MCP must not call this — see AwaitCompile.
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

	void ApplyLiveScriptStateSuccess(
		FUeremcpNiagaraCompileAwaitResult& Result,
		FNiagaraExt_SystemCompileState& OutState)
	{
		Result.bAwaited = true;
		Result.bObservedViaScriptState = true;
		Result.bLiveEnginePumpSkipped = true;
		if (OutState.bIsCompiling)
		{
			Result.bActiveQueueNotDrained = true;
			OutState.bIsCompiling = false;
		}
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

	if (!System)
	{
		Result.Error = TEXT(
			"AwaitCompile failed: UNiagaraSystem is null. "
			"Compile was not requested.");
		return Result;
	}

	if (!IsInGameThread())
	{
		Result.Error = TEXT(
			"AwaitCompile failed: must run on the game thread. "
			"Compile was not requested.");
		return Result;
	}

	if (!IsValid(System))
	{
		Result.Error = TEXT(
			"AwaitCompile failed: UNiagaraSystem is pending kill / invalid. "
			"Compile was not requested.");
		return Result;
	}

	const bool bLive = IsLiveToolDispatchContext();

	// Clear hybrid/stale ActiveCompilations before RequestCompile. RequestCompile may call
	// PollForCompilationComplete when Launch returns false with ActiveCompilations.Num()==1;
	// QueryCompileComplete on hybrid leftovers asserts TSharedPtr::operator->.
	// [VERIFIED: NiagaraSystem.cpp:3866-3868]
	// [VERIFIED: NiagaraSystem.cpp:3532-3548]
	// [VERIFIED: NiagaraSystem.h:458 KillAllActiveCompilations]
	if (bLive && System->HasActiveCompilations())
	{
		System->KillAllActiveCompilations();
	}

	// Stack input edits can leave the cached runtime SystemStateData unchanged when
	// Niagara considers the scripts otherwise up to date. A forced compile runs the
	// post-compile cache refresh before the generated package is saved.
	// [VERIFIED: NiagaraSystem.cpp:3608-3621]
	System->RequestCompile(true);

	if (!IsValid(System))
	{
		Result.Error = TEXT(
			"AwaitCompile failed: UNiagaraSystem became invalid after RequestCompile.");
		return Result;
	}

	const double Deadline = FPlatformTime::Seconds() + static_cast<double>(FMath::Max(1, TimeoutSeconds));
	while (FPlatformTime::Seconds() < Deadline)
	{
		if (!IsValid(System))
		{
			Result.Error = TEXT(
				"AwaitCompile failed: UNiagaraSystem became invalid while waiting for compile.");
			return Result;
		}

		UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);

		const bool bScriptStateComplete = IsScriptDerivedCompileComplete(OutState);
		if (bScriptStateComplete && (bLive || !OutState.bIsCompiling))
		{
			if (bLive)
			{
				ApplyLiveScriptStateSuccess(Result, OutState);
			}
			else
			{
				Result.bAwaited = true;
			}
			return Result;
		}

		if (bLive)
		{
			// Do NOT tick FTSTicker or ProcessAsyncTasks on live MCP/toolset dispatch.
			// Those pumps reach PollSystemCompilations → PollForCompilationComplete →
			// QueryCompileComplete and assert on invalid TSharedPtr task handles
			// (SharedPointer.h:1133). Observe script VM status only.
			// [VERIFIED: NiagaraModule.cpp:574-586]
			Result.bLiveEnginePumpSkipped = true;
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		PumpNiagaraCompileWait(/*bLimitExecutionTime=*/false);

		// Editor automation must keep polling until the script-derived state catches
		// up. One poll is insufficient after multiple stack-input edits: the first
		// pass can only promote the queued request into an active compile.
		if (System->HasActiveCompilations()
			|| System->HasOutstandingCompilationRequests(/*bIncludingGPUShaders=*/false))
		{
			System->PollForCompilationComplete(/*bFlushRequestCompile=*/false);
		}
		FPlatformProcess::Sleep(0.01f);
	}

	if (!IsValid(System))
	{
		Result.Error = TEXT(
			"AwaitCompile failed: UNiagaraSystem became invalid after compile timeout.");
		return Result;
	}

	UNiagaraExternalEditUtilities::GetSystemCompileState(System, OutState, Context);

	if (IsScriptDerivedCompileComplete(OutState)
		&& (bLive || !OutState.bIsCompiling))
	{
		if (bLive)
		{
			ApplyLiveScriptStateSuccess(Result, OutState);
		}
		else
		{
			Result.bAwaited = true;
		}
		return Result;
	}

	Result.bAwaited = !OutState.bIsCompiling;
	if (bLive)
	{
		Result.bLiveEnginePumpSkipped = true;
		Result.bObservedViaScriptState = true;
	}
	// Timeout / incomplete: leave Error empty so Create can soft-skip; Submit maps
	// !bCompiled to failed_validation. Never assert — live path already skipped pumps.
	return Result;
}
