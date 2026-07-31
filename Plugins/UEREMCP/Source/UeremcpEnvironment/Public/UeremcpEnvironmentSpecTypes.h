// UEREMCP — composable environment specification types (WS-16 v2).
#pragma once

#include "CoreMinimal.h"

/** Terrain generation profile — not tied to mountain/river/rain presets. */
enum class EUeremcpTerrainProfile : uint8
{
	Mountains,
	Plateau,
	Canyon,
	FlatWithMountainsRing,
};

/** One weather phenomenon (rain, snow, hail, fog). */
struct FUeremcpWeatherPhenomenonSpec
{
	FString Phenomenon;
	float Intensity = 0.5f;
	bool bFollowPlayer = true;
	FString FollowTarget = TEXT("player_camera");
	FString AssetPathOverride;
	TArray<FString> MaterialHints;
};

/** Declarative structure placement (ice walls, barriers, …). */
struct FUeremcpStructurePlacementSpec
{
	FString Kind;
	int32 Count = 12;
	float Height = 400.f;
	float Thickness = 200.f;
	float Width = 200.f;
	FString Placement = TEXT("terrain_bounds");
	FString MaterialPath;
};

inline FString TerrainProfileToString(EUeremcpTerrainProfile Profile)
{
	switch (Profile)
	{
	case EUeremcpTerrainProfile::Plateau: return TEXT("plateau");
	case EUeremcpTerrainProfile::Canyon: return TEXT("canyon");
	case EUeremcpTerrainProfile::FlatWithMountainsRing: return TEXT("flat_with_mountains_ring");
	default: return TEXT("mountains");
	}
}

inline bool ParseTerrainProfile(const FString& Raw, EUeremcpTerrainProfile& Out)
{
	const FString Key = Raw.ToLower();
	if (Key == TEXT("plateau")) { Out = EUeremcpTerrainProfile::Plateau; return true; }
	if (Key == TEXT("canyon")) { Out = EUeremcpTerrainProfile::Canyon; return true; }
	if (Key == TEXT("flat_with_mountains_ring")) { Out = EUeremcpTerrainProfile::FlatWithMountainsRing; return true; }
	if (Key == TEXT("mountains") || Key.IsEmpty()) { Out = EUeremcpTerrainProfile::Mountains; return true; }
	return false;
}

inline bool IsSupportedWeatherPhenomenon(const FString& Phenomenon)
{
	const FString Key = Phenomenon.ToLower();
	return Key == TEXT("rain") || Key == TEXT("snow") || Key == TEXT("hail") || Key == TEXT("fog");
}

inline bool IsSupportedStructureKind(const FString& Kind)
{
	const FString Key = Kind.ToLower();
	return Key == TEXT("ice_wall_ring") || Key == TEXT("barrier_wall") || Key == TEXT("box_along_river");
}
