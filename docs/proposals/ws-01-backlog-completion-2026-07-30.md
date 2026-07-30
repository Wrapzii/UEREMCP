# WS-01 backlog completion — 2026-07-30

**Branch:** `ws-01-backlog-integration`  
**Deploy/main tip (local):** `434bc12` then follow-up commit(s) on this branch  
**Dirty Opus root:** not modified (read-only copy of latest `COVERAGE_PLAN.md` + prototype echo)

## Deployment / SHA

| Ref | SHA | Notes |
|---|---|---|
| Baseline main/deploy | `82337de` | Pre-integration |
| Integration land | `434bc12` | Environment v0.1, echo, CaptureWorldFrames, ledger |
| Follow-up | _(this commit)_ | COVERAGE_PLAN Part III ops, plan handlers, GeometryScript structures |

RE junction: `...\RE\Plugins\UEREMCP` → `UEREMCP-deploy-main\Plugins\UEREMCP`.

## BACKLOG item ledger

See `docs/BACKLOG.md` Part "Backlog completion ledger".

## COVERAGE_PLAN delta (Part II–III)

Copied latest from dirty root (Parts II–III + III.1–III.11). Audited:

1. **GeometryScript "blocked"** — incorrect after enabling plugin. `AppendBox` present
   `[VERIFIED: MeshPrimitiveFunctions.h:168]`. `PlaceStructures` implemented.
2. **Module `UeremcpWorld`** — superseded by `UeremcpEnvironment` (WS-01 ownership).
3. **`attach_weather`** — added (gap called out in III.10).
4. **Plan composition** — `FUeremcpEnvironmentPlanHandlers` registers all stage
   actions with `FUeremcpPlanExecutor` (no second batching layer).
5. **`heightmap_hash`** — CRC of generated heights (III.4 / III.11.2).
6. **Exclusion re-measure** — post-scatter distance check (III.11.3).
7. **World capture** — `CaptureWorldFrames` (III.10 / BACKLOG 3.2).
8. **PCG** — audited; not duplicated for riverbank exclusion (W-DUP avoidance).
9. **Audio / networking** — documented limitation; **world partition** —
   `blocked_external` pending dedicated design.

Full capability table: `docs/COVERAGE_PLAN.md` Part IV.

## Commands / evidence

```text
python tools/validate_schemas.py          # OK (31 schemas)
# GeometryScripting + Water persisted in RE.uproject
# PluginToolset.IsEnabled GeometryScripting=true, Water=true [VERIFIED-RUNTIME]
# ALandscape::Import [VERIFIED: LandscapeProxy.h:1418-1420]
# AWaterBodyRiver [VERIFIED: WaterBodyRiverActor.h:28]
# GetWaterSpline [VERIFIED: WaterBodyActor.h:103]
# AppendBox [VERIFIED: MeshPrimitiveFunctions.h:168]
```

Live verification requires clean single-editor restart after UBT rebuild
(multi-editor MCP sessions previously hid GetStarted despite DLL exports).

## Acceptance scene

Target: `/Game/__UeremcpPoc/MountainRiverRain/`  
Path: `ResolveIntent` → `BuildEnvironment` (dry_run=false) → `ValidateEnvironment` /
`CaptureWorldFrames` (2–3 calls).

Telemetry and screenshots recorded after rebuild+live pass in this proposal's
follow-up section (or adjacent artifact under `Saved/UEREMCP/`).

## Remaining external limitations

- PIE camera-follow rain needs a project Niagara rain asset + movement transforms.
- World partition goal tooling: blocked pending design.
- Audio MetaSounds semantic tool: not started (Epic gap; no wrap of arbitrary execution).
- Focus mode still disabled until live describe+echo reconfirmed post-rebuild.
