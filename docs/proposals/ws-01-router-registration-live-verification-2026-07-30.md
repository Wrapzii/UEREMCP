# WS-01 router registration live verification — 2026-07-30

Validated integration tip before this evidence-only closeout:
`eed27a3b4628e2da4f2a4b31b7aa70288dea89a7`.
The final local `main` SHA is reported in the handoff after this document commit.
No remote push was performed.

## Consolidated branches

- `ws-03-discoverability-backlog` (`f033085`)
- `ws-01-remaining-domain-coverage` (`b87916a`)
- `ws-11-general-capture-validation` (`d0d4037`)
- `ws-16-environment-coverage` (`1e9574f`)
- concurrent MountainRiverRain closeout commits `a4e225c` and `1723e7d`

The RE plugin junction remained/restored to
`$UEREMCP_DEPLOY/Plugins/UEREMCP`.

## Build and live registry

`Build.bat REEditor Win64 Development -Project=.../RE.uproject
-NoHotReloadFromIDE -WaitMutex` returned `Result: Succeeded`.

Fresh live `list_toolsets` included:

- `UeremcpEnvironment.UeremcpEnvironmentToolset`
  (`0.3.0-environment-acceptance`)
- `UeremcpSystems.UeremcpSystemsToolset` (`0.1.0-coverage`)
- `UeremcpValidation.UeremcpVisualCaptureToolset`
  (`0.3.0-general-capture`)
- `UeremcpCore.UeremcpReferenceToolset`
  (`0.2.0-intent-router`)

`tools/dump_tool_registry.py` regenerated (not hand-edited)
`tools/registry_snapshot.json`: **82 toolsets, 978 tools, 42 source-declared
UEREMCP callables**, with `source_surface_fingerprint`.

Live describes exposed task vocabulary, complete request examples, and
`requestJson` input schemas for:

- Environment: `BuildEnvironment`, `CreateLandscape`, `CreateWaterBody`,
  `ScatterFoliage`, `AttachWeather`, `PlaceStructures`,
  `InspectEnvironment`, `ValidateEnvironment`
- Systems: `CreateAudioCue`, `InspectAudio`, `ValidateReplication`,
  `InspectWorldPartition`, `RepairWorldPartition`
- Capture: `CaptureEffectFrames`, `CaptureWorldFrames`,
  `CaptureMaterialFrames`, `CaptureAnimationFrames`

## Bootstrap and routing evidence

All exact names returned by live describe were callable:

- `GetStarted` → `status=no_change_required`,
  `next_call=ResolveIntent`
- `DescribeOperation(BuildEnvironment)` →
  `UeremcpEnvironment.UeremcpEnvironmentToolset.BuildEnvironment`, with
  usable dry-run request JSON
- `ResolveIntent("Make a landscape with mountains and a river, forest around
  the banks, raining...")` → `confidence=high`, `abstained=false`,
  **step 1/top-1 `BuildEnvironment`**, score `79`, with:

```json
{
  "protocol_version": "1.0",
  "action": "build_environment",
  "target": {
    "asset_path": "/Game/__UeremcpPoc/MountainRiverRain"
  },
  "options": {
    "dry_run": true,
    "validate": true
  },
  "specification": {
    "seed": 42
  }
}
```

The router previously returned low-score extra plan steps for that live request
(BACKLOG 1.3d). Tip now score-gates plan membership at 35% of best hit and caps
default `max_steps` at 3; offline mountain/river intent emits a **1-step**
`BuildEnvironment` plan. Live re-proof requires Core rebuild + editor reload.

**Ownership note:** IntentRouter / ReferenceToolset edits are WS-03 paths landed
on the integration tip to clear Tier-1 blocker 1.3d; see this proposal rather
than a silent cross-WS overwrite.

## Live safety and honesty

- Environment dry-run `BuildEnvironment`:
  `status=no_change_required`; summary included
  `Dry-run ... No actors spawned`; `height_range=0.7923157`,
  `non_flat=true`, `river_length=14575.48`.
- Systems dry-run `CreateAudioCue`, using existing
  `/Game/RE/Audio/_Shared/SW_Placeholder_Silent`:
  `status=partially_completed`; summary included
  `No assets written`; one wave + planned attenuation.
- `RepairWorldPartition` with no options:
  `status=partially_completed`; summary included
  `Dry-run ... No world mutated`. Mutation still requires explicit
  `options.dry_run=false` plus destructive opt-in.
- `CaptureWorldFrames` after the bounded PNG reread fix:
  `status=no_change_required`, `png_ok=true`,
  `png_files_reread=true`, `ok_frames=1`,
  `stage_teardown_complete=true`, `lit_pixels=56257`.

The previously accepted MountainRiverRain map remains:
`/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain`.
Structural and PIE weather-follow gates passed on WS-16. Rain particle visuals
remain honestly `approximated` because the project has no Niagara rain asset.

## Checks

- `validate_schemas.py`: **38/38 schemas valid**
- `check_tool_names.py`: **97 qualified references**, **0 problems**,
  **0 domain problems**
- `check_tool_selection_contract.py`: **36 tools inventoried**,
  **19/19 benchmark intents passed**
- `check_guide_links.py`: **101 relative links**, **38 path citations**, pass
- Python/unit suites: **25 tests**, then held-out fix rerun
  `tests.intent_router.test_intent_router_contract`: **16/16 pass**
- Held-out routing: **22 intents**; routable **19**; top-1 **16/19
  (84.21%)**; top-3 **19/19 (100%)**; MRR **0.8106**;
  confident-wrong **0**; abstention **3/3 (100%)**
- Focused editor automation: **11/11 pass**, **0 failed**, **0 skipped**:
  Environment **7**, Systems **3**, PNG reread honesty **1**
- Stale registry behavior:
  `test_snapshot_freshness_fails_closed_on_missing_callable` passes; the
  pre-refresh checker also rejected the stale snapshot with missing source
  callables before the live dump.

## Remaining limitations

- MetaSound graph authoring is not implemented; Systems creates SoundCue +
  optional attenuation only.
- `ValidateReplication` is a semantic audit/configure operation, not
  multi-client runtime proof.
- World Partition HLOD/builder commandlets remain blocked.
- Material and animation capture are live-discoverable but still need dedicated
  asset-specific live acceptance runs.
- World screenshots are supplemental and never replace structural validation.
