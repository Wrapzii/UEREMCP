// UEREMCP — execute_plan semantic handlers for the Systems domain.
//
// Being AICallable and being usable inside execute_plan are two separate
// registries. A tool present in one and absent from the other fails only at
// plan time -- "no handler registered for '<action>'" -- after the agent has
// already committed to a batch and has to abandon it.
//
// Found by tools/check_operation_catalog.py, which asserts every routable
// destructive catalog action has a binding here.

#include "UeremcpSystemsPlanHandlers.h"

#include "UeremcpSystemsToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FSystemsToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const TCHAR* Action,
		FSystemsToolFn Fn,
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

bool FUeremcpSystemsPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&OutError](const TCHAR* Action, FSystemsToolFn Fn) -> bool
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

	if (!Bind(TEXT("create_audio_cue"), &UUeremcpSystemsToolset::CreateAudioCue)) return false;
	if (!Bind(TEXT("inspect_audio"), &UUeremcpSystemsToolset::InspectAudio)) return false;
	if (!Bind(TEXT("validate_replication"), &UUeremcpSystemsToolset::ValidateReplication)) return false;
	if (!Bind(TEXT("inspect_world_partition"), &UUeremcpSystemsToolset::InspectWorldPartition)) return false;
	if (!Bind(TEXT("repair_world_partition"), &UUeremcpSystemsToolset::RepairWorldPartition)) return false;
	OutError.Reset();
	return true;
}

void FUeremcpSystemsPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(TEXT("create_audio_cue"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("inspect_audio"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("validate_replication"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("inspect_world_partition"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("repair_world_partition"));
}
