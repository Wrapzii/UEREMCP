// Offline tests for script-state compile await (WS-07 / MCP B1 B6).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UeremcpNiagaraCompileAwait.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FNiagaraExt_SystemCompileState MakeState(
		ENiagaraExt_ScriptCompileStatus Aggregate,
		std::initializer_list<ENiagaraExt_ScriptCompileStatus> ScriptStatuses,
		bool bHasErrors = false)
	{
		FNiagaraExt_SystemCompileState State;
		State.AggregateStatus = Aggregate;
		State.bHasErrors = bHasErrors;
		for (ENiagaraExt_ScriptCompileStatus ScriptStatus : ScriptStatuses)
		{
			FNiagaraExt_ScriptCompileInfo Script;
			Script.LastCompileStatus = ScriptStatus;
			State.Scripts.Add(Script);
		}
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCompileAwaitOfflineTest,
	"UEREMCP.Niagara.Create.CompileAwaitOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCompileAwaitOfflineTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("aggregate up to date"),
		FUeremcpNiagaraCompileAwait::IsAggregateCompileUpToDate(
			ENiagaraExt_ScriptCompileStatus::UpToDate));
	TestTrue(
		TEXT("aggregate up to date with warnings"),
		FUeremcpNiagaraCompileAwait::IsAggregateCompileUpToDate(
			ENiagaraExt_ScriptCompileStatus::UpToDateWithWarnings));

	const FNiagaraExt_SystemCompileState Complete = MakeState(
		ENiagaraExt_ScriptCompileStatus::UpToDate,
		{
			ENiagaraExt_ScriptCompileStatus::UpToDate,
			ENiagaraExt_ScriptCompileStatus::UpToDateWithWarnings,
		});
	TestTrue(
		TEXT("script derived complete"),
		FUeremcpNiagaraCompileAwait::IsScriptDerivedCompileComplete(Complete));

	const FNiagaraExt_SystemCompileState DirtyScript = MakeState(
		ENiagaraExt_ScriptCompileStatus::Dirty,
		{ENiagaraExt_ScriptCompileStatus::Dirty});
	TestFalse(
		TEXT("dirty script incomplete"),
		FUeremcpNiagaraCompileAwait::IsScriptDerivedCompileComplete(DirtyScript));

	const FNiagaraExt_SystemCompileState EmptyScripts;
	TestFalse(
		TEXT("empty scripts incomplete"),
		FUeremcpNiagaraCompileAwait::IsScriptDerivedCompileComplete(EmptyScripts));

	const FNiagaraExt_SystemCompileState ErrorFlag = MakeState(
		ENiagaraExt_ScriptCompileStatus::UpToDate,
		{ENiagaraExt_ScriptCompileStatus::UpToDate},
		/*bHasErrors=*/true);
	TestFalse(
		TEXT("error flag incomplete"),
		FUeremcpNiagaraCompileAwait::IsScriptDerivedCompileComplete(ErrorFlag));

	// Failure-mode documentation (no editor): live AwaitCompile must never pump
	// FTSTicker/ProcessAsyncTasks — those reach PollSystemCompilations →
	// QueryCompileComplete and assert TSharedPtr::operator-> IsValid()
	// (SharedPointer.h:1133) on hybrid ActiveCompilations. Structured Error is
	// returned instead; see FUeremcpNiagaraCompileAwaitResult::Error /
	// bLiveEnginePumpSkipped.
	{
		FUeremcpNiagaraCompileAwaitResult Doc;
		TestFalse(TEXT("default await result not awaited"), Doc.bAwaited);
		TestTrue(TEXT("default await result has empty Error"), Doc.Error.IsEmpty());
		TestFalse(TEXT("default live pump skipped false"), Doc.bLiveEnginePumpSkipped);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
