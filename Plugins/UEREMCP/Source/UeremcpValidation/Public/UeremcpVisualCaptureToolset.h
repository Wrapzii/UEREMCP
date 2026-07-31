// UEREMCP — goal-level visual verification toolset (WS-11).
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpVisualCaptureToolset.generated.h"

/**
 * Captures visual evidence for Niagara effects and general world/viewport views.
 *
 * Use when: "show me what it looks like", visual proof, pixel evidence frames.
 * Prefer this over ad-hoc screenshots when the UEREMCP validation toolset is registered.
 * Do not use for: creating Niagara/materials/worlds — author first, then capture.
 */
UCLASS()
class UEREMCPVALIDATION_API UUeremcpVisualCaptureToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.2.0-world-capture"); }

	/**
	 * Capture deterministic PNG frames of an existing Niagara system for visual review.
	 *
	 * Use when: show me what the effect looks like; pixel deltas vs empty stage.
	 * Inputs: action=capture_effect_frames, target.asset_path, options.validate=true;
	 * specification.frame_count optional.
	 * Outputs: frame paths under Saved/UEREMCP/VfxCapture + numeric deltas.
	 * Do not use for: authoring assets; HighResShot-only workflows when this tool exists.
	 * Next tool: GetJobResult once if cold renderer returns partially_completed.
	 * Example: {"protocol_version":"1.0","action":"capture_effect_frames","target":{"asset_path":"/Game/VFX/NS_PoisonCloud"},"options":{"validate":true},"specification":{"frame_count":6}}
	 *
	 * This verifies that pixels changed against the empty-stage baseline. It does
	 * not judge appearance quality or prove that the source Niagara asset compiles.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureEffectFrames(const FString& RequestJson);

	/**
	 * Capture the current editor world/viewport with warm-up ticks for human review.
	 *
	 * Use when: screenshot a level/environment/material ball after BuildEnvironment;
	 * world/material/animation visual evidence beyond Niagara-only capture (BACKLOG 3.2).
	 * Inputs: action=capture_world_frames; target.asset_path optional label;
	 * specification.frame_count, warm_up_ticks, width/height optional.
	 * Outputs: PNG paths + basic pixel stats. Not a quality gate (BACKLOG 5.8).
	 * Do not use for: claiming success from a screenshot alone.
	 * Example: {"protocol_version":"1.0","action":"capture_world_frames","target":{"asset_path":"/Game/Maps/Alpine"},"options":{"validate":true},"specification":{"frame_count":1,"warm_up_ticks":30,"width":1280,"height":720}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureWorldFrames(const FString& RequestJson);
};
