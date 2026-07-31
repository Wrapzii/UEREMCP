# WS-16 proposal — adopt environment audit and ownership

**To:** WS-01, WS-02. **From:** WS-16. **Date:** 2026-07-30.

Please adopt the capability rows and ownership assignment below into the shared
documents; WS-16 did not edit `docs/audit/**` or `docs/WORK_ALLOCATION.md`.

## Proposed audit rows

| Capability | Existing equivalent | Disposition | Why |
|---|---|---|---|
| Goal-level environment build | none in 911-tool live registry | add `BuildEnvironment` | cross-domain terrain/water/forest/weather constraints |
| Terrain | no landscape MCP tool | add heightmap import internally | deterministic; avoids unavailable brush APIs |
| Water | public Water runtime API, no tool wrapper | bind internally | real `AWaterBodyRiver`, not approximation |
| Forest scatter | Epic PCG + RE dress scatter | preserve; add bounded HISMC internally | existing tools do not measure river exclusion/both banks in one semantic call |
| World capture | RE capture workflow + UEREMCP `CaptureWorldFrames` | preserve | general capture is already solved; no new capture layer |
| Plan composition | UEREMCP `ExecutePlan`/job registry | preserve | environment actions register with the existing executor |

Evidence and API tags are in `docs/research/RB-16-environment-coverage.md`.

## Proposed ownership

Assign WS-16:

- `Plugins/UEREMCP/Source/UeremcpEnvironment/**`
- `schemas/domains/environment/**`
- `docs/research/RB-16-*.md`
- `docs/proposals/ws-16-*.md`

The user explicitly assigned the environment implementation to WS-16. Shared
catalog, plugin descriptor, router catalog, validation module, and project
configuration remain with their existing owners.

## Live acceptance evidence (2026-07-30)

See `tests/visual/MOUNTAIN_RIVER_RAIN_ACCEPTANCE.md`.

- Map `/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain` built via router→`BuildEnvironment` (seed 4471, revision `env:f49a66b5`).
- Structural gates passed including non-flat valley, continuous river, both-bank forest, open channel.
- PIE camera-follow rain gate passed (`weather_followed_10m=true`).
- Capture PNGs written under `tests/visual/mountain_river_rain/` (CaptureWorldFrames honesty mismatch is WS-11-owned).
- `check_ownership.py --ws WS-16` currently fails because WS-16 is not in `WORK_ALLOCATION.md` yet.
