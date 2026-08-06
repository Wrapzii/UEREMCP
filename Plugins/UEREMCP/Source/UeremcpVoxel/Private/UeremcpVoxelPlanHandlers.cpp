#include "UeremcpVoxelPlanHandlers.h"

#include "UeremcpVoxelToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FVoxelToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const FString& Action,
		FVoxelToolFn Fn,
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
			TEXT("carve_spline"),
			TEXT("flatten_area"),
			TEXT("smooth_region"),
			TEXT("terrain_stamp"),
			TEXT("noise_sculpt"),
			TEXT("paint_material"),
			TEXT("generate_water_body"),
			TEXT("procedural_scatter"),
			TEXT("generate_pois"),
			TEXT("compose_interior_terrain"),
		};
		return Names;
	}
}

bool FUeremcpVoxelPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&](const FString& Action, FVoxelToolFn Fn) -> bool
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

	if (!Bind(TEXT("carve_spline"), &UUeremcpVoxelToolset::CarveSpline)) return false;
	if (!Bind(TEXT("flatten_area"), &UUeremcpVoxelToolset::FlattenArea)) return false;
	if (!Bind(TEXT("smooth_region"), &UUeremcpVoxelToolset::SmoothRegion)) return false;
	if (!Bind(TEXT("terrain_stamp"), &UUeremcpVoxelToolset::TerrainStamp)) return false;
	if (!Bind(TEXT("noise_sculpt"), &UUeremcpVoxelToolset::NoiseSculpt)) return false;
	if (!Bind(TEXT("paint_material"), &UUeremcpVoxelToolset::PaintMaterial)) return false;
	if (!Bind(TEXT("generate_water_body"), &UUeremcpVoxelToolset::GenerateWaterBody)) return false;
	if (!Bind(TEXT("procedural_scatter"), &UUeremcpVoxelToolset::ProceduralScatter)) return false;
	if (!Bind(TEXT("generate_pois"), &UUeremcpVoxelToolset::GeneratePois)) return false;
	if (!Bind(TEXT("compose_interior_terrain"), &UUeremcpVoxelToolset::ComposeInteriorTerrain)) return false;
	OutError.Reset();
	return true;
}

void FUeremcpVoxelPlanHandlers::Unregister()
{
	for (const FString& Name : ActionNames())
	{
		FUeremcpPlanExecutor::UnregisterAction(Name);
	}
}
