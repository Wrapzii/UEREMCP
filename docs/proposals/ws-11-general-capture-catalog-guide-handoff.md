# WS-11: general visual capture + acceptance tooling

**Owner:** WS-11  
**Recipients:** WS-01 (catalog/schemas/BACKLOG/COVERAGE/VISUAL_CAPTURE_PROTOCOL), WS-13 (guide)  
**Branch:** `ws-11-general-capture-validation`  
**Status:** Static/BuildPlugin/unit ready; **live RE deferred** (environment sibling owns editor).

## Delivered (this branch — WS-11 owned paths)

| Item | Path / action | State |
|---|---|---|
| Shared warm-up / framing / PNG reread / path safety / teardown | `UeremcpVisualCaptureCommon.*` | implemented |
| World capture generalized | `capture_world_frames` | request-id isolation + structural snapshot |
| Material capture | `capture_material_frames` | implemented; live pending |
| Animation capture | `capture_animation_frames` | implemented; live pending |
| GetSystemSummary fail-soft regression | `UEREMCP.Validation.Niagara.GetSystemSummaryFailSoft.MissingAsset` | editor automation |
| Mountain–river–rain harness | `tests/visual/mountain_river_rain_harness.py` | offline evaluator |
| Material/animation protocols | `tests/visual/*_CAPTURE_PROTOCOL.md` | docs |
| Semantic eval telemetry schema | `tests/schemas/semantic_eval_report.schema.json` | tests-owned |
| Live handoff | `tests/visual/LIVE_INTEGRATION_CHECKLIST.md` | prepared |

## Requested WS-01 ledger updates (do not edit from WS-11)

Please apply these honest states to owned shared docs:

### `docs/BACKLOG.md`

| ID | Suggested final state | Evidence |
|---|---|---|
| 3.2 | `completed_with_documented_limitation` | world/material/animation capture on this branch; live RE pending |
| 3.3 | `completed_with_documented_limitation` | Niagara fail-soft + WS-11 Validation missing-asset regression; throwaway crash isolation still open |

### `docs/COVERAGE_PLAN.md` Part IV

| ID | Suggested state | Evidence |
|---|---|---|
| CP-III.10-capture | `completed_with_documented_limitation` | structural world snapshot; beauty not gated |
| CP-III.10-material-capture | `completed_with_documented_limitation` | `capture_material_frames`; live pending |
| CP-III.10-animation-capture | `completed_with_documented_limitation` | `capture_animation_frames`; live pending |
| CP-III.10-world-acceptance | `completed_with_documented_limitation` | offline harness; needs isolated-editor evidence |
| CP-III.10-eval-telemetry | `completed_and_verified` (unit) | `tests/schemas/semantic_eval_report.schema.json` |

### `docs/VISUAL_CAPTURE_PROTOCOL.md`

Point status at toolset `0.3.0-general-capture`, shared common helpers, and
`tests/visual/{MATERIAL,ANIMATION}_CAPTURE_PROTOCOL.md` + live checklist.

### Catalog / schemas

1. Catalog entries (honest `partial` until live):
   `capture_world_frames`, `capture_material_frames`, `capture_animation_frames`
2. Optional promote of `tests/schemas/semantic_eval_report.schema.json` into
   `schemas/domains/validation/`.
3. Specification schemas under `schemas/domains/validation/` for the three actions
   (mirror existing `capture_effect_frames.schema.json`).

## Requested WS-13 changes

Document worked requests from `tests/visual/MATERIAL_CAPTURE_PROTOCOL.md` and
`ANIMATION_CAPTURE_PROTOCOL.md` in `docs/guide/**`, stressing B10 supplemental
screenshots.

## Live integration checklist (do not run while sibling owns RE)

See `tests/visual/LIVE_INTEGRATION_CHECKLIST.md`. Summary:

1. Rebuild + restart; `list_toolsets` version `0.3.0-general-capture`.
2. Dry-run all four capture actions → no files.
3. IceWall / material / animation / world proofs with structural + supplemental PNGs.
4. Missing Niagara inspect fail-soft (no crash).
5. Mountain-river harness on evidence dir; emit `semantic_eval_report`.

## Limitations (honest)

- No live RE proof on this branch by design.
- Animation capture requires `specification.skeletal_mesh_path`.
- World beauty remains human-reviewed; harness measures structure.
- Do not mark catalog `available` until live checklist passes.
