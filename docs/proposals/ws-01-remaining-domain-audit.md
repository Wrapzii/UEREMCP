# WS-01 — Remaining domain coverage audit rows (for WS-02)

Supply for `docs/audit/` — WS-02 owns the file; do not squat.

## Epic / REAgentTools equivalence

| Gap | Existing tools (911 registry) | Disposition | Why insufficient | Proposed UEREMCP action |
|---|---|---|---|---|
| Audio | 1 keyword hit: Sequencer `set_clock_source` (AUDIO clock) — **not** MetaSound/SoundCue authoring. Zero MetaSound toolsets. REAgentTools: none. | **add** goal-level | No create/inspect for SoundCue/attenuation | `create_audio_cue`, `inspect_audio` |
| Networking | `BlueprintTools.get_variable_replication` / `set_variable_replication` (2). GameplayCue "non-replicated" execute (1). | **compose → improve** | Primitive per-variable loop; no Pattern B batch audit; no honest multi-client gate | `validate_replication` (static + optional batch configure). Multi-client remains WS-11 |
| World Partition | **0** registry tools matching partition/streaming/HLOD | **add** inspect/repair; **block** HLOD builders | Create/repair exists in engine C++ but is unreachable via MCP | `inspect_world_partition`, `repair_world_partition` (dry_run default). HLOD/`WorldPartitionBuilderCommandlet` blocked |
| PCG (non-env) | 31 PCG tools | **preserve** | Environment owns scatter; remaining stream must not duplicate | See `ws-01-pcg-coordination.md` |

## Verified engine APIs (UE 5.8)

- `USoundCueFactoryNew::InitialSoundWaves` `[VERIFIED: SoundCueFactoryNew.h:44-45]`
- `USoundAttenuationFactory` `[VERIFIED: SoundAttenuationFactory.h:21-28]`
- `USoundBase::AttenuationSettings` `[VERIFIED: SoundBase.h:221-222]`
- `FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags` `[VERIFIED: BlueprintEditorUtils.h:1236]`
- `FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc` `[VERIFIED: BlueprintEditorUtils.h:1245]`
- `UWorldPartition::CreateOrRepairWorldPartition` `[VERIFIED: WorldPartition.h:167]`
- `UWorldPartition::SetEnableStreaming` / `IsStreamingEnabled` `[VERIFIED: WorldPartition.h:175,470]`
- `UWorld::GetWorldPartition` / `IsPartitionedWorld` `[VERIFIED: World.h:2955,2968]`
- HLOD builders via `WorldPartitionBuilderCommandlet` — **blocked** as agent-facing mutate (commandlet-oriented, not a single validated goal op yet)

## Thin-wrapper ban

`validate_replication` must **not** be marketed as a rename of
`get_variable_replication`. It returns Pattern B + all expected variables in one
response. Agents should not loop Epic primitives for the same goal.
