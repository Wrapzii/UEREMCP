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
	 * Use when: add cast/impact audio; make a sound cue with attenuation; wire waves into a cue.
	 * Inputs: action=create_audio_cue; target.asset_path; specification.sound_waves REQUIRED;
	 * volume_multiplier / pitch_multiplier / create_attenuation optional.
	 * Outputs: cue path + attenuation binding. Prefer options.dry_run=true first.
	 * Do not use for: MetaSound graph authoring (blocked — no goal API yet).
	 * Next tool: InspectAudio; create_spell presentation.audio_* soft paths; ValidateReplication.
	 * Example: {"protocol_version":"1.0","action":"create_audio_cue","target":{"asset_path":"/Game/__UeremcpTests/Audio/SC_Cast"},"options":{"dry_run":true},"specification":{"sound_waves":["/Game/Audio/SW_Cast"],"create_attenuation":true}}
	 *
	 * @param RequestJson Envelope with action create_audio_cue.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|Audio")
	static FString CreateAudioCue(const FString& RequestJson);

	/**
	 * Inspect a SoundCue / SoundWave / SoundAttenuation asset in one response.
	 *
	 * Use when: verify audio asset wiring; list waves; check attenuation binding.
	 * Inputs: action=inspect_audio; target.asset_path; specification has no required keys
	 * (include_wave_paths optional).
	 * Outputs: asset class, wave paths, attenuation soft path.
	 * Do not use for: MetaSound graph inspection.
	 * Example: {"protocol_version":"1.0","action":"inspect_audio","target":{"asset_path":"/Game/__UeremcpTests/Audio/SC_Cast"},"specification":{"include_wave_paths":true}}
	 *
	 * @param RequestJson Envelope with action inspect_audio.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|Audio")
	static FString InspectAudio(const FString& RequestJson);

	/**
	 * Goal-level Blueprint replication audit (+ optional configure) in one call.
	 *
	 * Use when: ensure variables are replicated / RepNotify; Pattern B checklist; replication validation.
	 * Inputs: action=validate_replication; target.asset_path Blueprint;
	 * specification.networking and/or specification.variables REQUIRED (anyOf); apply_fixes optional.
	 * Outputs: match/mismatch table. Multi-client runtime proof remains WS-11 — never claimed here.
	 * Do not use for: looping BlueprintTools.get/set_variable_replication as the agent surface.
	 * Example: {"protocol_version":"1.0","action":"validate_replication","target":{"asset_path":"/Game/__UeremcpTests/BP_Rep"},"options":{"dry_run":true},"specification":{"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"},"variables":[{"name":"Health","replication":"REPLICATED"}]}}
	 *
	 * @param RequestJson Envelope with action validate_replication.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|Networking")
	static FString ValidateReplication(const FString& RequestJson);

	/**
	 * Read-only world-partition state for the editor world or a loaded level.
	 *
	 * Use when: is this map partitioned?; streaming enabled?; world partition bounds / actor-desc count.
	 * Inputs: action=inspect_world_partition; target.asset_path optional (omit = editor world);
	 * specification has no required keys (include_bounds optional).
	 * Outputs: is_partitioned, streaming_enabled, bounds summary.
	 * Do not use for: mutating WP / HLOD builds.
	 * Example: {"protocol_version":"1.0","action":"inspect_world_partition","specification":{"include_bounds":true}}
	 *
	 * @param RequestJson Envelope with action inspect_world_partition.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|WorldPartition")
	static FString InspectWorldPartition(const FString& RequestJson);

	/**
	 * Create-or-repair World Partition on the current editor world (or named level).
	 *
	 * Use when: enable world partition on a large procedural world before streaming validation.
	 * Inputs: action=repair_world_partition; target.asset_path optional; specification.enable_streaming
	 * optional. Defaults to options.dry_run=true. Mutating requires dry_run=false + allow_destructive.
	 * Outputs: planned/applied CreateOrRepairWorldPartition result. HLOD builders blocked.
	 * Do not use for: silent commandlet wraps; HLOD construction.
	 * Example: {"protocol_version":"1.0","action":"repair_world_partition","options":{"dry_run":true},"specification":{"enable_streaming":true}}
	 *
	 * @param RequestJson Envelope with action repair_world_partition.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Systems|WorldPartition")
	static FString RepairWorldPartition(const FString& RequestJson);
};
