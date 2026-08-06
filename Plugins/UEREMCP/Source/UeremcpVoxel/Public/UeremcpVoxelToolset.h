#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpVoxelToolset.generated.h"

/**
 * High-level VoxelFree terrain ops for interior dungeon landscapes
 * (valleys, rivers, fake mountains, village pads inside a carved vault).
 *
 * Prefer ComposeInteriorTerrain for "valley + river + village".
 * Prefer CarveSpline / FlattenArea / NoiseSculpt when staging via ExecutePlan.
 * Do not chain thousands of RemoveSphere calls — these tools use box/level stamps.
 */
UCLASS()
class UEREMCPVOXEL_API UUeremcpVoxelToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-voxel-terrain"); }

	/**
	 * Carve a tunnel / river trench along a polyline using AABB box capsules.
	 *
	 * Use when: connect chambers; dig a river bed; replace sphere-chain tunnels.
	 * Inputs: action=carve_spline; target.actor_label = VoxelWorld label (or omit for first VW);
	 *   specification.points REQUIRED (world cm [[x,y,z],...]); radius_cm; optional
	 *   cross_section=box|capsule; floor_bias_cm; step_factor.
	 * Example: {"protocol_version":"1.0","action":"carve_spline","target":{"actor_label":"VW_RE_ProcCave"},"options":{"dry_run":false},"specification":{"points":[[0,0,0],[2000,0,-100],[4000,500,-200]],"radius_cm":180,"cross_section":"capsule"}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString CarveSpline(const FString& RequestJson);

	/**
	 * Flatten / level a circular pad (village foundation, plaza, road).
	 *
	 * Use when: need a flat buildable disk inside voxel terrain.
	 * Inputs: action=flatten_area; center_cm; radius_cm; height_cm; additive optional.
	 * Example: {"protocol_version":"1.0","action":"flatten_area","target":{"actor_label":"VW_RE_ProcCave"},"specification":{"center_cm":[1000,2000,50],"radius_cm":1200,"height_cm":250}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString FlattenArea(const FString& RequestJson);

	/**
	 * Smooth a region with a sparse SmoothSphere grid.
	 *
	 * Use when: soften box-carve seams after stamp / spline / noise.
	 * Inputs: action=smooth_region; center_cm; radius_cm; strength; iterations.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString SmoothRegion(const FString& RequestJson);

	/**
	 * Stamp a procedural heightfield (mountains / hills) via coarse column boxes.
	 *
	 * Use when: raise fake mountains on a flat vault floor without sphere spam.
	 * Inputs: action=terrain_stamp; origin_cm; size_xy_cm; amplitude_cm; seed; optional
	 *   cell_cm; mode=add|carve.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString TerrainStamp(const FString& RequestJson);

	/**
	 * Noise sculpt = seeded fBm heightfield + optional valley groove.
	 *
	 * Use when: interior open-world floor with ridge mountains and a river valley.
	 * Inputs: action=noise_sculpt; origin_cm; size_xy_cm; seed; amplitude_cm;
	 *   valley_axis=x|y|none; valley_width_cm; valley_depth_cm.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString NoiseSculpt(const FString& RequestJson);

	/**
	 * Paint voxel material channel in a sphere or box.
	 *
	 * Use when: riverbed / biome layer paint on voxel surface.
	 * Inputs: action=paint_material; shape=sphere|box; channel; center+radius OR min/max.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString PaintMaterial(const FString& RequestJson);

	/**
	 * Spawn a WaterBody (river/lake) along points — visual water over a carved trench.
	 *
	 * Use when: river surface after CarveSpline trench. Does not sculpt voxels.
	 * Inputs: action=generate_water_body; points; body_type=river|lake; label; water_z_cm.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString GenerateWaterBody(const FString& RequestJson);

	/**
	 * Scatter static meshes on an XY pad with optional downward snap traces.
	 *
	 * Use when: trees/rocks/props on interior terrain.
	 * Inputs: action=procedural_scatter; origin_cm; size_xy_cm; mesh_paths; count; seed.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString ProceduralScatter(const FString& RequestJson);

	/**
	 * Generate points of interest (flatten pads + markers / buildings).
	 *
	 * Use when: village / shrine / camp sites inside the vault.
	 * Inputs: action=generate_pois; pois=[{kind,center_cm,radius_cm,mesh_path,label},...].
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString GeneratePois(const FString& RequestJson);

	/**
	 * One-shot interior open-world: noise mountains + valley + river + village + scatter.
	 *
	 * Use when: "make a valley with a river and a village" inside a dungeon vault.
	 * Inputs: action=compose_interior_terrain; origin_cm; size_xy_cm; seed;
	 *   optional river/village/scatter blocks (auto_river/auto_village default true).
	 * Example: {"protocol_version":"1.0","action":"compose_interior_terrain","target":{"actor_label":"VW_RE_ProcCave"},"options":{"dry_run":false},"specification":{"origin_cm":[0,0,-500],"size_xy_cm":[12000,12000],"seed":7,"amplitude_cm":900,"scatter_meshes":["/Game/RE/Caves/Dress/SM_Pine"]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Voxel")
	static FString ComposeInteriorTerrain(const FString& RequestJson);
};
