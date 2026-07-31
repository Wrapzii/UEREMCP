# WS-01 — Live editor handoff: remaining domain coverage

**Branch:** `ws-01-remaining-domain-coverage`  
**Do not** retarget the RE junction while environment live work is running. Deploy
this plugin only into a **separate** editor / project when ready.

## Preflight (BuildPlugin / static)

```text
python tools/validate_schemas.py
python tools/check_ownership.py --ws WS-01   # expects WS-03 proposal for .uplugin module line
python -m unittest schemas.domains.audio.test_remaining_domain_schemas
# From Engine (requires free UBT mutex — concurrent env builds block this):
RunUAT.bat BuildPlugin -Plugin="<repo>/Plugins/UEREMCP/UEREMCP.uplugin" -Package="<tmp>/UEREMCPSystemsPkg" -Rocket
```

**2026-07-30 note:** BuildPlugin on this branch hit `ConflictingInstance` (UBT mutex held by
concurrent environment live work). Do not kill that build. Re-run BuildPlugin when the
mutex is free; do not retarget the RE junction for this verification.
Automation (editor):

```text
Ueremcp.Systems.Networking.PatternBChecklist
Ueremcp.Systems.Audio.ParseCreateSpec
Ueremcp.Systems.Networking.ParseValidateSpec
```

## Live MCP checks (after clean single-editor load of this plugin)

1. `list_toolsets` contains `UeremcpSystems.UeremcpSystemsToolset`.
2. `describe_toolset` shows CreateAudioCue / InspectAudio / ValidateReplication /
   InspectWorldPartition / RepairWorldPartition with task vocabulary.
3. Dry-run create:

```json
{
  "protocol_version": "1.0",
  "action": "create_audio_cue",
  "target": {"asset_path": "/Game/__UeremcpTests/Audio/SC_Handoff"},
  "specification": {
    "sound_waves": ["<existing SoundWave path>"],
    "create_attenuation": true
  },
  "options": {"dry_run": true}
}
```

Expect `partially_completed`, no assets written.

4. `validate_replication` dry-run against a scratch Blueprint with Pattern B + one
   variable expectation — expect match/mismatch table in one response.
5. `inspect_world_partition` with empty target (editor world) — expect
   `is_partitioned` / `streaming_enabled` fields.
6. `repair_world_partition` with default options — must **not** mutate (dry_run
   safety). Mutate only with `dry_run:false` + `allow_destructive:true` on a
   scratch map.

## Honesty

- Never claim multi-client replication validated from step 4.
- Never claim MetaSound graph authored.
- Never claim HLOD build from step 6.
