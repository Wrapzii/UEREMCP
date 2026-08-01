// UEREMCP — goal-level environment / world toolset (WS-01).
// Module name UeremcpEnvironment supersedes COVERAGE_PLAN's provisional UeremcpWorld
// (same operations; clearer domain name). COVERAGE_PLAN III ledger records this.
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpEnvironmentToolset.generated.h"

/**
 * Build and verify procedural environments (landscape, river, forest, weather, structures).
 *
 * Prefer BuildEnvironment with explicit include.* flags for each subsystem (all default false).
 * Prefer CreateLandscape / CreateWaterBody / ScatterFoliage / AttachWeather when
 * composing via ExecutePlan with $ref chaining (COVERAGE_PLAN III.8 / III.10).
 * Use ResolveIntent when unsure which world tool to call.
 */
UCLASS()
class UEREMCPENVIRONMENT_API UUeremcpEnvironmentToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.4.0-environment-v2"); }

	/**
	 * Build a seeded environment with composable v2 blocks or legacy include.* flags.
	 *
	 * Use when: one call for a whole biome (terrain + water + foliage + weather).
	 *   Prefer the staging primitives when the caller enumerated the steps, or when
	 *   any stage needs its own parameters.
	 * Inputs: action=build_environment; specification.seed REQUIRED. Optional blocks:
	 *   options.on_unsupported = fail (default) | partial. Use partial when you want
	 *   every supported part applied and the rest reported, rather than the whole
	 *   request rejected because one block is unsupported.
	 *   terrain.profile, hydrology.river, vegetation.mode, weather[], structures[].
	 * Example: {"protocol_version":"1.0","action":"build_environment","target":{"asset_path":"/Game/__UeremcpPoc/Biome"},"options":{"dry_run":true},"specification":{"seed":42,"terrain":{"profile":"mountains","scale_z":3}}}
	 *
	 * v2 (presence-based): terrain.profile, hydrology.river, vegetation.mode, weather[],
	 * structures[], lighting.preset — each block activates its subsystem.
	 * Legacy: include.* all default false (418374c opt-in contract preserved).
	 *
	 * Multi-weather: snow+hail each get CreateNiagaraEffect precipitation assets; no streak fake.
	 * Structures: ice_wall_ring places GeometryScript boxes around terrain bounds.
	 *
	 * Example (snow/ice/hail): see docs/research/RB-16-environment-coverage.md § Snow acceptance
	 * Example (legacy MRR): include.terrain/river/forest/rain true with biome + river blocks.
	 * Next: ValidateEnvironment; CaptureWorldFrames; or GetJobResult if partially_completed.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString BuildEnvironment(const FString& RequestJson);

	/**
	 * Create terrain from a seeded heightmap via ALandscape::Import (not sculpt brushes).
	 *
	 * Use when: generate mountains/valley heightmap only; ExecutePlan terrain stage.
	 * Inputs: action=create_landscape; specification.seed REQUIRED; terrain.* optional.
	 *   terrain.scale_z: SET THIS. Sane range is 2-5. The default of 100 produces
	 *   vertical needles, not mountains [VERIFIED-RUNTIME: three separate builds].
	 *   It is a Z multiplier, not a metre value, and the default is wrong for every
	 *   terrain profile shipped. Start at 3. To make peaks higher raise
	 *   terrain.max_altitude_m; raising scale_z past ~5 gives spikes.
	 * Outputs: heightmap_hash (determinism gate), structural_metrics.
	 * Do not use for: AlphaBrush sculpting — unavailable [VERIFIED-RUNTIME: COVERAGE_PLAN III.1].
	 * Example: {"protocol_version":"1.0","action":"create_landscape", "target":{"asset_path":"/Game/__UeremcpPoc/Biome"}, "options":{"dry_run":true},"specification":{"seed":42, "terrain":{"profile":"mountains","scale_z":3,"max_altitude_m":1800}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString CreateLandscape(const FString& RequestJson);

	/**
	 * Create a WaterBody (river, lake, or ocean) without wiping other water types.
	 *
	 * Use when: river through a valley; lake / ocean for still water; additive multi-water.
	 * Inputs: action=create_water_body; specification.seed REQUIRED;
	 *   body_type=river|lake|ocean (default river); optional label, river.width,
	 *   lake.{center,radius_cm}, ocean.{center,extents_cm}.
	 * Outputs: water actor with type-specific label (UEREMCP_River|Lake|Ocean or custom).
	 *   Only the matching label is replaced — other water bodies remain.
	 * Do not use for: inventing body_type values; unknown types are rejected
	 *   (BODY_TYPE_UNSUPPORTED), never silently coerced to a river.
	 * Example: {"protocol_version":"1.0","action":"create_water_body",
	 *   "specification":{"seed":42,"body_type":"lake","lake":{"center":[8500,3200,-80],"radius_cm":2500}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString CreateWaterBody(const FString& RequestJson);

	/**
	 * Seeded instanced foliage with slope/bank masks and river exclusion corridor.
	 *
	 * Use when: forest along river banks with a clear channel; ExecutePlan foliage stage.
	 * Inputs: action=scatter_foliage; specification.seed; biome.mesh_path; max_foliage_instances.
	 * Outputs: foliage_instances, exclusion_violations (must be 0).
	 * Cross-domain: exclusions reference the river spline (why BuildEnvironment is C++ not only a template).
	 * Multiple species: set vegetation.group (or biome.group) to a distinct name per
	 *   species. A scatter replaces only its OWN group, so a second call ADDS a
	 *   species instead of wiping the first. Omitting it uses one shared group and
	 *   each scatter replaces the last.
	 * A river is optional. With hydrology.river the forest bands along the bank and
	 *   keeps a clear channel; without one it scatters across the terrain under the
	 *   slope and height gates alone. Trees on a bare hillside no longer need a river.
	 * Honesty: when biome.mesh_path is absent or unloadable this places
	 *   /Engine/BasicShapes/Cube as a placeholder and warns. Supply a real mesh,
	 *   or generate one first, if you want trees rather than boxes.
	 * Example: {"protocol_version":"1.0","action":"scatter_foliage", "target":{"asset_path":"/Game/__UeremcpPoc/Biome"}, "options":{"dry_run":true},"specification":{"seed":42, "biome":{"mesh_path":"/Game/Meshes/SM_Pine"},"max_foliage_instances":5000}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString ScatterFoliage(const FString& RequestJson);

	/**
	 * Attach rain/fog weather that can follow the player/camera.
	 *
	 * Use when: "make it raining including on the player camera"; ExecutePlan weather stage.
	 * Inputs: action=attach_weather; specification.seed REQUIRED;
	 *   weather.rain_system_path optional when fallback_policy=allow_approximate;
	 *   follow=player_camera.
	 * Outputs: weather actors. PIE camera-follow proof requires rain system asset + movement check.
	 * Do not use for: day/night or weather-state cycles — no cycle contract exists;
	 *   this attaches a single state.
	 * Example: {"protocol_version":"1.0","action":"attach_weather", "target":{"asset_path":"/Game/__UeremcpPoc/Biome"}, "options":{"dry_run":true},"specification":{"seed":42, "weather":{"kind":"rain","follow":"player_camera"}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString AttachWeather(const FString& RequestJson);

	/**
	 * Place procedural structures along a spline using GeometryScript AppendBox.
	 *
	 * Use when: "village by the river" props/kits; ExecutePlan structures stage.
	 * Inputs: action=place_structures; specification.seed; count optional.
	 * Requires GeometryScripting enabled [VERIFIED-RUNTIME: IsEnabled=true 2026-07-30].
	 * Do not use for: calling static-mesh kit drops "procedural" without GeometryScript.
	 *   Also not for keeps/towers/gatehouses — kind accepts
	 *   ice_wall_ring|barrier_wall|box_along_river only, and an unknown kind is
	 *   REJECTED rather than substituted.
	 * Example: {"protocol_version":"1.0","action":"place_structures", "target":{"asset_path":"/Game/__UeremcpPoc/Biome"}, "options":{"dry_run":true},"specification":{"seed":42, "structures":[{"kind":"barrier_wall","count":12}]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString PlaceStructures(const FString& RequestJson);

	/**
	 * Author a StaticMesh asset from ordered GeometryScript primitive ops.
	 *
	 * Use when: you need BLOCKOUT geometry — a crate, a wall segment, a pillar, a
	 *   collision proxy — or a stand-in that unblocks layout while real art is
	 *   sourced. This is the mesh PRIMITIVE FLOOR: it consumes no existing asset.
	 * NOT FOR HERO ART. Boxes, cylinders, cones and spheres cannot look like a
	 *   photograph. A cylinder with a cone on top reads as a tree ICON, not a tree.
	 *   If the caller said realistic, high quality, hero, or matching a reference,
	 *   use editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file and
	 *   bring in a real model. Shipping primitives against a quality ask is the
	 *   failure this note exists to prevent.
	 * Inputs: action=submit_mesh_ops, target.asset_path (StaticMesh under
	 *   /Game/__UeremcpTests/ or /Game/__UeremcpPoc/); specification.ops is a
	 *   REQUIRED non-empty array applied in order. Each op is one of:
	 *   {"op":"box","size":[x,y,z],"origin":[x,y,z]},
	 *   {"op":"cylinder","radius":r,"height":h},
	 *   {"op":"cone","base_radius":r,"top_radius":r2,"height":h},
	 *   {"op":"sphere","radius":r}.
	 * Outputs: primary_asset (the StaticMesh). An unsupported op REJECTS the whole
	 *   request — a partially built mesh is indistinguishable from a correct one.
	 * Do not use for: placing meshes in a level — use ScatterFoliage or
	 *   PlaceStructures with the asset path this returns.
	 * Next tool: PlaceStructures, or ScatterFoliage via biome.mesh_path — but only
	 *   when blockout foliage is genuinely what was asked for.
	 * Example: {"protocol_version":"1.0","action":"submit_mesh_ops","target":{"asset_path":"/Game/__UeremcpTests/Meshes/SM_CrateBlockout"},"options":{"dry_run":true},"specification":{"ops":[{"op":"box","size":[120,120,120]},{"op":"box","size":[130,130,12],"origin":[0,0,120]}]}}
	 *
	 * @param RequestJson  Request with action submit_mesh_ops and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString SubmitMeshOps(const FString& RequestJson);

	/**
	 * Snap actors down onto the LANDSCAPE surface, ignoring foliage.
	 *
	 * Use when: buildings, props or roads sit above or below the terrain after a
	 *   rebuild or an import. This is the fix for floating actors.
	 * Inputs: action=snap_actors_to_landscape; specification.label_prefixes is an
	 *   optional array (default ["UEREMCP_"]); specification.z_offset_cm optional.
	 * Do not use for: foliage instances -- rescatter those instead.
	 * Why it exists: a generic downward trace stops on the first blocker, which in
	 *   a forest is a tree, which is how buildings ended up resting on canopy.
	 *   This traces ALandscapeProxy only. An actor with no landscape beneath it is
	 *   NAMED and left alone rather than dropped into the void.
	 * Example: {"protocol_version":"1.0","action":"snap_actors_to_landscape","options":{"dry_run":true},"specification":{"label_prefixes":["UEREMCP_Structure","Castle"],"z_offset_cm":0}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString SnapActorsToLandscape(const FString& RequestJson);

	/**
	 * Remove instanced-foliage instances inside world-space boxes.
	 *
	 * Use when: clearing a building footprint, a road corridor or a courtyard so
	 *   trees stop growing through walls.
	 * Inputs: action=clear_foliage_in_volumes; specification.volumes REQUIRED --
	 *   a non-empty array of {"center":[x,y,z],"extent":[x,y,z]} in world space.
	 * Do not use for: WaterBodyExclusionVolume is NOT a foliage cull. It excludes
	 *   water. Placing one and expecting trees to disappear is why they did not.
	 * Example: {"protocol_version":"1.0","action":"clear_foliage_in_volumes","options":{"dry_run":true},"specification":{"volumes":[{"center":[1800,-2000,9000],"extent":[1500,1500,4000]}]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString ClearFoliageInVolumes(const FString& RequestJson);

	/**
	 * Probe the AssetRegistry for meshes matching semantic roles.
	 *
	 * Use when: you need a real foliage/structure mesh that EXISTS in this project
	 *   before scattering or placing. This is why agents stopped inventing
	 *   /Game/Meshes/SM_Pine.
	 * Inputs: action=find_project_assets; specification.roles REQUIRED array of
	 *   semantic roles (foliage.tree, foliage.grass, structure.wall, …). Optional
	 *   class_filter, path_scopes, max_per_role.
	 * Outputs: resolved[] with real paths, unresolved[] with searched patterns and
	 *   satisfied_by import action. Never fabricates a path.
	 * Do not use for: importing a file — use import_mesh_for_world when unresolved.
	 * Example: {"protocol_version":"1.0","action":"find_project_assets","request_id":"fa-1","specification":{"roles":["foliage.tree"],"class_filter":["StaticMesh"],"path_scopes":["/Game"],"max_per_role":5}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString FindProjectAssets(const FString& RequestJson);

	/**
	 * Import a mesh for world use: file → unit check → collision → Nanite.
	 *
	 * Use when: bringing an FBX/OBJ into /Game for foliage or prefabs. Composes
	 *   StaticMeshTools.import_file only as fallback; primary path is silent
	 *   UAssetImportTask(bAutomated) so Interchange Import Content never opens.
	 * Inputs: action=import_mesh_for_world; target.asset_path; specification.source_file
	 *   REQUIRED. Optional source_unit, collision, nanite, expected_bounds_m.
	 *   expected_bounds_m is the load-bearing field — reject when actual bounds
	 *   differ by more than ~20%.
	 * Example: {"protocol_version":"1.0","action":"import_mesh_for_world","target":{"asset_path":"/Game/__UeremcpPoc/Meshes/SM_Castle"},"specification":{"source_file":"C:/exports/castle.fbx","source_unit":"meters","collision":"complex_as_simple","nanite":false,"expected_bounds_m":[240,145,90]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString ImportMeshForWorld(const FString& RequestJson);

	/**
	 * Place a static-mesh prefab on the landscape: spawn + snap + clear foliage.
	 *
	 * Use when: dropping a building/prop that must sit on terrain, not canopy.
	 * Inputs: action=place_prefab_on_landscape; specification.mesh_path,
	 *   location_xy, optional rotation_yaw, clear_foliage_radius_cm, flatten_pad.
	 *   flatten_pad writes a soft heightmap pad via FLandscapeEditDataInterface::SetHeightData.
	 * Example: {"protocol_version":"1.0","action":"place_prefab_on_landscape","specification":{"mesh_path":"/Game/__UeremcpPoc/Meshes/SM_Castle","location_xy":[1800,-2000],"rotation_yaw":35,"clear_foliage_radius_cm":1500,"flatten_pad":{"radius_cm":1200,"falloff_cm":600}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString PlacePrefabOnLandscape(const FString& RequestJson);

	/**
	 * Paint landscape layer weights from height/slope rules on the LIVE landscape.
	 *
	 * Use when: terrain material layers exist but the ground is still the first
	 *   layer (the white-landscape failure). Reads height/slope from the landscape
	 *   itself — never recomputes a heightmap.
	 * Inputs: action=paint_landscape_layers; specification.rules REQUIRED. Exactly
	 *   one rule may set fallback=true; weights sum to 1 per vertex.
	 * Example: {"protocol_version":"1.0","action":"paint_landscape_layers","specification":{"rules":[{"layer":"snow","min_height_m":1100,"blend_m":150},{"layer":"rock","min_slope_deg":35,"blend_deg":8},{"layer":"grass","fallback":true}]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString PaintLandscapeLayers(const FString& RequestJson);

	/**
	 * Read-only inspect of environment actors/metrics in the current editor world.
	 *
	 * Use when: diagnostics after BuildEnvironment / ExecutePlan.
	 * Inputs: action=inspect_environment; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"inspect_environment","target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain"},"specification":{}}
	 * During PIE returns weather_follow_distance_cm and weather_followed_10m.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString InspectEnvironment(const FString& RequestJson);

	/**
	 * Validate structural gates (landscape, forest, river, rain). Screenshots are never a gate.
	 *
	 * Use when: acceptance after build; human-review companion metrics (COVERAGE_PLAN 5.8 / III.11).
	 * Inputs: action=validate_environment; specification.require_weather_follow_10m optional.
	 * Example: {"protocol_version":"1.0","action":"validate_environment","target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain"},"specification":{"require_weather_follow_10m":true}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString ValidateEnvironment(const FString& RequestJson);
};
