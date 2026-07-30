#include "UeremcpPlanTransactionCoordinator.h"

#include "Misc/Guid.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "ToolsetRegistry/ToolsetLibrary.h"
#include "UeremcpPlanExecutor.h"

namespace UeremcpPlanTransactionPrivate
{
	struct FSessionState
	{
		bool bActive = false;
		FString SandboxName;
		int32 UndoCountBefore = 0;
	};

	static FSessionState& Session()
	{
		static FSessionState State;
		return State;
	}

	static void UndoEditorDeltaToBaseline(int32 BaselineUndoCount)
	{
		while (UToolsetLibrary::GetActiveUndoCount() > BaselineUndoCount)
		{
			if (!UToolsetLibrary::UndoTransaction(/*bCanRedo=*/false))
			{
				break;
			}
		}
	}
}

bool FUeremcpPlanTransactionCoordinator::RegisterWithExecutor(FString& OutError)
{
	FUeremcpPlanTransactionCallbacks Callbacks;
	Callbacks.Begin = [](FString& Error) { return Begin(Error); };
	Callbacks.Commit = [](FString& Error) { return Commit(Error); };
	Callbacks.Rollback = [](FString& Error) { return Rollback(Error); };
	return FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Callbacks), OutError);
}

void FUeremcpPlanTransactionCoordinator::UnregisterFromExecutor()
{
	if (IsSessionActive())
	{
		FString Error;
		Rollback(Error);
	}
	FUeremcpPlanExecutor::ClearTransactionCallbacks();
}

bool FUeremcpPlanTransactionCoordinator::IsSessionActive()
{
	return UeremcpPlanTransactionPrivate::Session().bActive;
}

bool FUeremcpPlanTransactionCoordinator::Begin(FString& OutError)
{
	using namespace UE::ToolsetRegistry;
	UeremcpPlanTransactionPrivate::FSessionState& State = UeremcpPlanTransactionPrivate::Session();

	if (State.bActive)
	{
		OutError = TEXT("execute_plan transaction session already active");
		return false;
	}

	if (FGlobalSandbox::IsActive())
	{
		OutError = FString::Printf(
			TEXT("cannot begin execute_plan transaction: sandbox '%s' is already active"),
			*FGlobalSandbox::GetActiveName());
		return false;
	}

	State.SandboxName = FString::Printf(
		TEXT("execute_plan_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	if (!FGlobalSandbox::Enter(
			State.SandboxName,
			TEXT("UEREMCP atomic execute_plan batch")))
	{
		State.SandboxName.Reset();
		OutError = TEXT("FGlobalSandbox::Enter failed for execute_plan transaction");
		return false;
	}

	State.UndoCountBefore = UToolsetLibrary::GetActiveUndoCount();
	State.bActive = true;
	return true;
}

bool FUeremcpPlanTransactionCoordinator::Commit(FString& OutError)
{
	using namespace UE::ToolsetRegistry;
	UeremcpPlanTransactionPrivate::FSessionState& State = UeremcpPlanTransactionPrivate::Session();

	if (!State.bActive)
	{
		OutError = TEXT("no active execute_plan transaction session to commit");
		return false;
	}

	if (!FGlobalSandbox::IsActive())
	{
		OutError = TEXT("execute_plan transaction sandbox is not active at commit");
		State.bActive = false;
		State.SandboxName.Reset();
		return false;
	}

	const TArray<FString> PersistAll;
	if (!FGlobalSandbox::Persist(PersistAll))
	{
		OutError = TEXT("FGlobalSandbox::Persist failed for execute_plan transaction");
		return false;
	}

	if (!FGlobalSandbox::Leave())
	{
		OutError = TEXT("FGlobalSandbox::Leave failed after execute_plan persist");
		return false;
	}

	State.bActive = false;
	State.SandboxName.Reset();
	return true;
}

bool FUeremcpPlanTransactionCoordinator::Rollback(FString& OutError)
{
	using namespace UE::ToolsetRegistry;
	UeremcpPlanTransactionPrivate::FSessionState& State = UeremcpPlanTransactionPrivate::Session();

	if (!State.bActive)
	{
		OutError = TEXT("no active execute_plan transaction session to roll back");
		return false;
	}

	const int32 BaselineUndo = State.UndoCountBefore;
	bool bSandboxOk = true;

	if (FGlobalSandbox::IsActive())
	{
		bSandboxOk = FGlobalSandbox::Discard();
		if (bSandboxOk)
		{
			bSandboxOk = FGlobalSandbox::Leave();
		}
	}

	UeremcpPlanTransactionPrivate::UndoEditorDeltaToBaseline(BaselineUndo);

	State.bActive = false;
	State.SandboxName.Reset();

	if (!bSandboxOk)
	{
		OutError = TEXT("FGlobalSandbox discard/leave failed during execute_plan rollback");
		return false;
	}

	return true;
}
