# Live integration checklist — WS-11 general capture

**Branch:** `ws-11-general-capture-validation`  
**Constraint:** Do **not** retarget the RE junction or start the editor while an
environment sibling owns the live RE process. This checklist is for the handoff
owner after they take the project.

## Preflight

- [ ] Sibling confirms RE editor released.
- [ ] Deploy this branch's `Plugins/UEREMCP` to the editor plugin path (junction or copy).
- [ ] `RunUAT BuildPlugin` (or project rebuild) completed with this SHA.
- [ ] Editor restart; MCP session reconnected.

## Registration

- [ ] `list_toolsets` → `UeremcpValidation.UeremcpVisualCaptureToolset`
- [ ] Toolset version `0.3.0-general-capture`
- [ ] Four AICallable tools present

## Contract smoke

- [ ] Dry-run `capture_effect_frames` / `capture_world_frames` /
      `capture_material_frames` / `capture_animation_frames`
- [ ] Missing Niagara inspect → fail-soft (no crash)
- [ ] Outputs only under `Saved/UEREMCP/{Vfx,World,Material,Animation}Capture/`

## Domain proofs

- [ ] Niagara known-good system: pixel delta + PNG reread + teardown
- [ ] Material: structural identity + supplemental PNGs
- [ ] Animation: play_length + bone count + supplemental PNGs
- [ ] World: structural actor/world fields + mountain_river harness on evidence dir

## Report

- [ ] Write `semantic_eval_report` (schema `tests/schemas/semantic_eval_report.schema.json`)
- [ ] Overall never `*_validated` from screenshots alone
- [ ] Update BACKLOG 3.x / COVERAGE CP-III.10 rows to `completed_and_verified` only after above
