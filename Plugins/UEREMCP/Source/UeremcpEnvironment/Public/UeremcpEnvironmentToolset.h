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
 * Prefer BuildEnvironment for the full mountain/river/forest/rain scene.
 * Prefer CreateLandscape / CreateWaterBody / ScatterFoliage / AttachWeather when
 * composing via ExecutePlan with $ref chaining (COVERAGE_PLAN III.8 / III.10).
 * Use ResolveIntent when unsure which world tool to call.
 */
UCLASS()
class UEREMCPENVIRONMENT_API UUeremcpEnvironmentToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.3.0-environment-acceptance"); }

	/**
	 * Build a seeded environment: mountains, river, forest banks, rain, lighting, optional capture.
	 *
	 * Use when: "make a landscape with mountains and a river, forest around it, raining, screenshot".
	 * Inputs: action=build_environment; target under /Game/__UeremcpPoc/; specification.seed REQUIRED;
	 * options.dry_run must be true for preflight; false creates, saves, reloads, and structurally validates.
	 * Example: {"protocol_version":"1.0","action":"build_environment","request_id":"env-1","target":{"asset_path":"/Game/__UeremcpPoc/MountainRiverRain/"},"idempotency_key":"mountain-river-rain-v1","options":{"dry_run":true,"validate":true,"save":true},"specification":{"seed":4471,"terrain":{"size_x":127,"size_y":127,"mountain_amplitude":0.6,"valley_depth":0.18},"river":{"width":900},"biome":{"max_foliage_instances":1000,"slope_limit_deg":32},"weather":{"follow":"player_camera"},"fallback_policy":"allow_approximate"}}
	 * Next: ValidateEnvironment; CaptureWorldFrames; or GetJobResult if partially_completed.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString BuildEnvironment(const FString& RequestJson);

	/**
	 * Create terrain from a seeded heightmap via ALandscape::Import (not sculpt brushes).
	 *
	 * Use when: generate mountains/valley heightmap only; ExecutePlan terrain stage.
	 * Inputs: action=create_landscape; specification.seed REQUIRED; terrain.* optional.
	 * Outputs: heightmap_hash (determinism gate), structural_metrics.
	 * Do not use for: AlphaBrush sculpting — unavailable [VERIFIED-RUNTIME: COVERAGE_PLAN III.1].
	 * Example: {"protocol_version":"1.0","action":"create_landscape","target":{"asset_path":"/Game/__UeremcpPoc/Alpine"},"options":{"dry_run":true},"specification":{"seed":4471}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString CreateLandscape(const FString& RequestJson);

	/**
	 * Create a WaterBody river/lake/ocean from spline points.
	 *
	 * Use when: add a river through a valley; ExecutePlan water stage after landscape.
	 * Inputs: action=create_water_body; specification.seed; river.width; body_type optional.
	 * Outputs: river actor label + spline length. Real AWaterBodyRiver when Water plugin loaded.
	 * Example: {"protocol_version":"1.0","action":"create_water_body","target":{"asset_path":"/Game/__UeremcpPoc/Alpine"},"options":{"dry_run":true},"specification":{"seed":4471,"river":{"width":1200}}}
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
	 * Example: {"protocol_version":"1.0","action":"scatter_foliage","target":{"asset_path":"/Game/__UeremcpPoc/Alpine"},"options":{"dry_run":true},"specification":{"seed":4471,"biome":{"mesh_path":"/Game/Meshes/SM_Pine","max_foliage_instances":800}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString ScatterFoliage(const FString& RequestJson);

	/**
	 * Attach rain/fog weather that can follow the player/camera.
	 *
	 * Use when: "make it raining including on the player camera"; ExecutePlan weather stage.
	 * Inputs: action=attach_weather; weather.rain_system_path optional; follow=player_camera.
	 * Outputs: weather actors. PIE camera-follow proof requires rain system asset + movement check.
	 * Specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"attach_weather","target":{"asset_path":"/Game/__UeremcpPoc/Alpine"},"options":{"dry_run":true},"specification":{"weather":{"rain_system_path":"/Game/VFX/NS_Rain"},"follow":"player_camera"}}
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
	 * Example: {"protocol_version":"1.0","action":"place_structures","target":{"asset_path":"/Game/__UeremcpPoc/Alpine"},"options":{"dry_run":true},"specification":{"seed":4471,"count":6}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString PlaceStructures(const FString& RequestJson);

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
