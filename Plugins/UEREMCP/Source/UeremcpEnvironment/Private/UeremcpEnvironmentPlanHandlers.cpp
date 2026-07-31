// UEREMCP — execute_plan handlers for environment domain (COVERAGE_PLAN III.3 / III.8).

#include "UeremcpEnvironmentPlanHandlers.h"

#include "UeremcpEnvironmentToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FEnvToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const FString& Action,
		FEnvToolFn Fn,
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = Fn(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s returned an empty response"), *Action);
			return false;
		}
		return true;
	}

	const TArray<FString>& ActionNames()
	{
		static const TArray<FString> Names = {
			TEXT("build_environment"),
			TEXT("create_landscape"),
			TEXT("create_water_body"),
			TEXT("scatter_foliage"),
			TEXT("attach_weather"),
			TEXT("place_structures"),
			TEXT("inspect_environment"),
			TEXT("validate_environment"),
			TEXT("submit_mesh_ops"),
			TEXT("snap_actors_to_landscape"),
			TEXT("clear_foliage_in_volumes"),
		};
		return Names;
	}
}

bool FUeremcpEnvironmentPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&](const FString& Action, FEnvToolFn Fn) -> bool
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

	if (!Bind(TEXT("build_environment"), &UUeremcpEnvironmentToolset::BuildEnvironment)) return false;
	if (!Bind(TEXT("create_landscape"), &UUeremcpEnvironmentToolset::CreateLandscape)) return false;
	if (!Bind(TEXT("create_water_body"), &UUeremcpEnvironmentToolset::CreateWaterBody)) return false;
	if (!Bind(TEXT("scatter_foliage"), &UUeremcpEnvironmentToolset::ScatterFoliage)) return false;
	if (!Bind(TEXT("attach_weather"), &UUeremcpEnvironmentToolset::AttachWeather)) return false;
	if (!Bind(TEXT("place_structures"), &UUeremcpEnvironmentToolset::PlaceStructures)) return false;
	if (!Bind(TEXT("inspect_environment"), &UUeremcpEnvironmentToolset::InspectEnvironment)) return false;
	if (!Bind(TEXT("validate_environment"), &UUeremcpEnvironmentToolset::ValidateEnvironment)) return false;
	// Mesh authoring is the first operation in a from-scratch plan. Without
	// this line execute_plan rejects the whole batch with "no handler for
	// submit_mesh_ops" and the agent falls back to one call per mesh.
	if (!Bind(TEXT("submit_mesh_ops"), &UUeremcpEnvironmentToolset::SubmitMeshOps)) return false;
	if (!Bind(TEXT("snap_actors_to_landscape"), &UUeremcpEnvironmentToolset::SnapActorsToLandscape)) return false;
	if (!Bind(TEXT("clear_foliage_in_volumes"), &UUeremcpEnvironmentToolset::ClearFoliageInVolumes)) return false;
	OutError.Reset();
	return true;
}

void FUeremcpEnvironmentPlanHandlers::Unregister()
{
	for (const FString& Name : ActionNames())
	{
		FUeremcpPlanExecutor::UnregisterAction(Name);
	}
}
