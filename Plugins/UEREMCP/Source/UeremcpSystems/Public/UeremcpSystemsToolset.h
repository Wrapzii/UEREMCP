// UEREMCP — goal-level audio / networking / world-partition surface (WS-01).
//
// Epic registry has effectively no MetaSound/audio toolset, only Blueprint
// get/set_variable_replication for networking, and zero World Partition tools.
// This module adds semantic ops; it does not re-expose those primitives.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpSystemsToolset.generated.h"

/**
 * Remaining-domain coverage toolset (audio, replication validation, world partition).
 *
 * Prefer CreateAudioCue over inventing MetaSound graph primitives.
 * Prefer ValidateReplication over looping BlueprintTools.get/set_variable_replication.
 * Prefer InspectWorldPartition / RepairWorldPartition over silent WP commandlet wraps.
 * Use ResolveIntent if unsure.
 */
UCLASS()
class UEREMCPSYSTEMS_API UUeremcpSystemsToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-coverage"); }

	/**
	 * Create or update a SoundCue (optional attenuation) from SoundWave paths.
	 *
	 * Use when: "add cast/impact audio", "make a sound cue with attenuation".
	 * Example request: action=create_audio_cue, target.asset_path=/Game/__UeremcpTests/Audio/SC_Cast,
	 * specification={ "sound_waves": ["/Game/.../SW_Cast"], "volume_multiplier": 1.0,
	 * "create_attenuation": true }.
	 * Prefer options.dry_run=true first. Destructive replace defaults require allow_destructive.
	 * Do not use for: MetaSound graph authoring (blocked — no goal API yet).
	 * Next tool: create_spell presentation.audio_* soft paths, or validate_replication.
	 *
	 * @param RequestJson Envelope with action create_audio_cue.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|Audio")
	static FString CreateAudioCue(const FString& RequestJson);

	/**
	 * Inspect a SoundCue / SoundWave / SoundAttenuation asset in one response.
	 *
	 * Use when: verify audio asset wiring, list waves, check attenuation binding.
	 *
	 * @param RequestJson Envelope with action inspect_audio.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|Audio")
	static FString InspectAudio(const FString& RequestJson);

	/**
	 * Goal-level Blueprint replication audit (+ optional configure) in one call.
	 *
	 * Use when: "ensure these variables are replicated / RepNotify", Pattern B check.
	 * Composes BlueprintEditorUtils property flags — does not thin-wrap
	 * BlueprintTools.get/set_variable_replication as the agent surface.
	 * Multi-client runtime proof remains WS-11/RB-14 and is never claimed here.
	 *
	 * @param RequestJson Envelope with action validate_replication.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|Networking")
	static FString ValidateReplication(const FString& RequestJson);

	/**
	 * Read-only world-partition state for the editor world or a loaded level.
	 *
	 * Use when: "is this map partitioned?", streaming enabled?, bounds / actor-desc count.
	 *
	 * @param RequestJson Envelope with action inspect_world_partition.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|WorldPartition")
	static FString InspectWorldPartition(const FString& RequestJson);

	/**
	 * Create-or-repair World Partition on the current editor world (or named level).
	 *
	 * Use when: enable WP on a large procedural world before streaming validation.
	 * Defaults to options.dry_run=true. Mutating requires dry_run=false.
	 * HLOD / builder commandlets are blocked — see capability_notes.
	 *
	 * @param RequestJson Envelope with action repair_world_partition.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|WorldPartition")
	static FString RepairWorldPartition(const FString& RequestJson);
};
