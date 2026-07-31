// UEREMCP — goal-level visual verification toolset (WS-11).
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpVisualCaptureToolset.generated.h"

/**
 * Captures visual evidence for Niagara, world/viewport, materials, and animation.
 *
 * Screenshots are supplemental. Structural evidence (asset identity, compile/inspect
 * summaries, pixel stats, teardown) is the gate. Prefer this over ad-hoc HighResShot
 * loops when the validation toolset is registered.
 */
UCLASS()
class UEREMCPVALIDATION_API UUeremcpVisualCaptureToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override
	{
		return TEXT("0.3.0-general-capture");
	}

	/**
	 * Capture deterministic PNG frames of an existing Niagara system.
	 *
	 * Use when: show me what the effect looks like; pixel deltas vs empty stage.
	 * Inputs: action=capture_effect_frames, target.asset_path, options.validate=true;
	 * specification.frame_count / duration_seconds / camera / width / height optional.
	 * Outputs: frame paths under Saved/UEREMCP/VfxCapture + numeric deltas.
	 * Do not use for: authoring assets; appearance judgements from pixels alone.
	 * Next tool: GetJobResult if cold renderer returns partially_completed.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureEffectFrames(const FString& RequestJson);

	/**
	 * Capture the current editor world with warm-up ticks and framed SceneCapture2D.
	 *
	 * Use when: screenshot a level/environment after BuildEnvironment (BACKLOG 3.2).
	 * Inputs: action=capture_world_frames; target.asset_path optional label;
	 * specification.frame_count, warm_up_ticks, camera, width/height optional.
	 * Outputs: PNG paths + pixel stats + structural world snapshot. Not a beauty gate.
	 * Do not use for: claiming success from a screenshot alone (POC_ACCEPTANCE B10).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureWorldFrames(const FString& RequestJson);

	/**
	 * Capture a material on a disposable lit stage with structural material evidence.
	 *
	 * Use when: visual proof of a VFX/landscape material after create_vfx_material.
	 * Inputs: action=capture_material_frames, target.asset_path = material path;
	 * specification.frame_count / camera / width / height / warm_up_ticks optional.
	 * Outputs: PNG + luminance stats + structural material identity. Supplemental only.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureMaterialFrames(const FString& RequestJson);

	/**
	 * Capture an animation sequence posed on a skeletal mesh at deterministic times.
	 *
	 * Use when: visual proof of a montage/sequence after animation authoring.
	 * Inputs: action=capture_animation_frames, target.asset_path = AnimSequence;
	 * specification.skeletal_mesh_path required unless mesh is on the sequence;
	 * frame_count / duration_seconds / camera / width / height optional.
	 * Outputs: PNG + bone/length structural evidence. Supplemental to compile gates.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureAnimationFrames(const FString& RequestJson);
};
