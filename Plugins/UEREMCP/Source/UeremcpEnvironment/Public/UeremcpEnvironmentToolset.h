// UEREMCP — goal-level environment / world toolset (WS-01).
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpEnvironmentToolset.generated.h"

/**
 * Build and verify procedural environments (landscape, river, forest, weather).
 *
 * Prefer BuildEnvironment over hundreds of SceneTools/ActorTools placements.
 * Use ResolveIntent when unsure which world tool to call.
 * Do not use for: Blueprint/Niagara/Material authoring — those stay in domain toolsets.
 */
UCLASS()
class UEREMCPENVIRONMENT_API UUeremcpEnvironmentToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-backlog"); }

	/**
	 * Build a seeded environment: mountains, river, forest banks, rain, lighting, optional capture.
	 *
	 * Use when: "make a landscape with mountains and a river, forest around it, raining, screenshot";
	 * create a new scratch world under /Game/__UeremcpPoc/.
	 * Inputs: action=build_environment; target.asset_path (level or package root);
	 * specification.seed (required for determinism); terrain/river/biome/weather/lighting/viewpoint/capture;
	 * options.dry_run defaults true; options.validate recommended; idempotency_key recommended.
	 * Outputs: real_vs_approximated, change_manifest, structural_metrics, warnings, screenshots, honest status.
	 * Do not use for: sculpt-stroke editing; wrapping arbitrary Python; deleting user assets outside scratch.
	 * Next tool: GetJobResult if partially_completed; ValidateEnvironment after save; CaptureWorldFrames for extra views.
	 *
	 * Internally batches via ExecutePlan semantics (terrain → river → foliage → weather → capture).
	 * Does not introduce a second batching layer (BACKLOG 5.7 / ADR-0009).
	 *
	 * @param RequestJson Request envelope (schemas/domains/environment/build_environment.schema.json).
	 * @return Response envelope with validation and technology honesty notes.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString BuildEnvironment(const FString& RequestJson);

	/**
	 * Read-only inspect of an environment package: landscape metrics, water, foliage counts, weather actors.
	 *
	 * Use when: "what is in this world?", verify mountain/river/forest after BuildEnvironment.
	 * Inputs: action=inspect_environment, target.asset_path.
	 * Do not use for: mutating the world — use BuildEnvironment.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString InspectEnvironment(const FString& RequestJson);

	/**
	 * Validate environment structural gates (non-flat terrain, continuous river, bank foliage, rain presence).
	 *
	 * Use when: acceptance checks after BuildEnvironment; human-review companion metrics.
	 * Inputs: action=validate_environment, target.asset_path, specification.gates optional.
	 * Outputs: pass/fail per gate — screenshots alone are never success (BACKLOG 5.8).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Environment")
	static FString ValidateEnvironment(const FString& RequestJson);
};
