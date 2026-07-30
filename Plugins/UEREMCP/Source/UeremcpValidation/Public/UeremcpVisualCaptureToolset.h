// UEREMCP — goal-level visual verification toolset (WS-11).
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpVisualCaptureToolset.generated.h"

/**
 * Captures an existing Niagara system; it does not author or compile assets.
 *
 * Use when: "show me what it looks like", visual proof, pixel evidence frames.
 * Prefer this over ad-hoc screenshots when the UEREMCP validation toolset is registered.
 * Do not use for: creating Niagara/materials — author first, then capture.
 */
UCLASS()
class UEREMCPVALIDATION_API UUeremcpVisualCaptureToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.1-intent-vocab"); }

	/**
	 * Capture deterministic PNG frames of an existing Niagara system for visual review.
	 *
	 * Use when: show me what the effect looks like; pixel deltas vs empty stage.
	 * Inputs: action=capture_effect_frames, target.asset_path, options.validate=true;
	 * specification.frame_count optional.
	 * Outputs: frame paths under Saved/UEREMCP/VfxCapture + numeric deltas.
	 * Do not use for: authoring assets; HighResShot-only workflows when this tool exists.
	 * Next tool: GetJobResult once if cold renderer returns partially_completed.
	 *
	 * This verifies that pixels changed against the empty-stage baseline. It does
	 * not judge appearance quality or prove that the source Niagara asset compiles.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureEffectFrames(const FString& RequestJson);
};
