// UEREMCP — execute_plan semantic handlers for the Blueprint domain.
//
// Being AICallable and being usable inside execute_plan are two separate
// registries. A tool present in one and absent from the other fails only at
// plan time -- "no handler registered for '<action>'" -- after the agent has
// already committed to a batch and has to abandon it.
//
// Found by tools/check_operation_catalog.py, which asserts every routable
// destructive catalog action has a binding here.
//
// submit_graph is the notable one: the graph round-trip that the whole
// document model is modelled on could not appear in a batch, so any plan
// that authored an asset and then wired logic for it had to split in two.

#include "UeremcpBlueprintPlanHandlers.h"

#include "UeremcpBlueprintToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FBlueprintToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const TCHAR* Action,
		FBlueprintToolFn Fn,
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = Fn(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s returned an empty response"), Action);
			return false;
		}
		return true;
	}
}

bool FUeremcpBlueprintPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&OutError](const TCHAR* Action, FBlueprintToolFn Fn) -> bool
	{
		FString LocalError;
		const bool bOk = FUeremcpPlanExecutor::RegisterAction(
			Action,
			[Action, Fn](const FString& RequestJson, FString& OutResponseJson, FString& Err) -> bool
			{
				return DispatchTool(Action, Fn, RequestJson, OutResponseJson, Err);
			},
			LocalError);
		if (!bOk)
		{
			OutError = LocalError;
		}
		return bOk;
	};

	if (!Bind(TEXT("read_graph"), &UUeremcpBlueprintToolset::ReadGraph)) return false;
	if (!Bind(TEXT("submit_graph"), &UUeremcpBlueprintToolset::SubmitGraph)) return false;
	OutError.Reset();
	return true;
}

void FUeremcpBlueprintPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(TEXT("read_graph"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("submit_graph"));
}
