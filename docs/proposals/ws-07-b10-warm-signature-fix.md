# WS-07 B10 warm-signature diagnosis and material handoff

Status: Niagara-side correction complete; B10 remains blocked by generated materials.

## Root cause

The production system is alive and on camera, but its six generated fire materials do
not produce a warm viewport contribution. The pre-fix production run observed roughly
422 live particles and 715 total spawned particles with zero warm changed pixels. The
known-good canary produced 43 warm pixels under the same gate, excluding the backdrop,
pixel classifier, and viewport capture as the cause.

The Niagara scaffold also did not explicitly write the semantic `primary_color` into
`Particles.Color`. Renderers already consumed `Particles.Color`, so WS-07 corrected the
generator by adding one particle-spawn assignment per generated emitter. The assignment
uses the fixture's warm `[1.0, 0.12, 0.01, 1.0]` value. `AddSetParametersModule` is the
supported external-edit operation for this assignment.
[VERIFIED: Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:1367-1369]

After that correction and a fresh create, B10 still reported 412 live particles, 715
spawned particles, 5,405 changed pixels, and zero warm changed pixels. This excludes
zero emission, off-camera placement, and wholly sub-pixel sizing. The screenshot is
visually black apart from the viewport origin gizmo. Since the generated system now
writes an opaque warm particle color and every color-capable renderer re-reads
`Particles.Color`, the remaining failure is downstream in the generated material
output/binding, not Niagara particle simulation.
[VERIFIED-RUNTIME: `tests/run_poc_b10_visible_render.ps1`, log
`tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_093352.log`]

The additional stack edits exposed a compile-await race in editor automation: one poll
could promote a queued compile without draining it. WS-07 now polls until the
script-derived state is complete or the existing deadline expires.
[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraCompileAwait.cpp]

## Evidence

- Fresh create gate:
  `UEREMCP_POC_B_FIREBALL_OUTCOME=PASS proof=editor_single_create_inline_materials_expanded_gates`
  [VERIFIED-RUNTIME:
  `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_093241.log`]
- Production B10:
  `UEREMCP_POC_B10_OUTCOME=FAIL reason=visible_fire_signature_not_observed`
  [VERIFIED-RUNTIME:
  `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_093352.log`]
- Evidence payload:
  `changed_pixels=5405`, `warm_changed_pixels=0`, `particle_count=412`,
  `total_spawned_particles=715`, `runtime_emitter_instances=6`.
- Screenshot:
  `tests/integration/_artifacts/poc_b10_fireball.png`.
- Fixture requests fire-oriented generated masters/MIs for all six roles.
  [VERIFIED: schemas/domains/niagara/fixtures/poc_b_fireball_materials.json:4-54]

## WS-08 handoff

Please diagnose the generated masters and instances used by:

- `MI_NS_POCB_Fireball_core`
- `MI_NS_POCB_Fireball_flame_shell`
- `MI_NS_POCB_Fireball_sparks`
- `MI_NS_POCB_Fireball_smoke`
- `MI_NS_POCB_Fireball_ribbon_trail`
- `MI_NS_POCB_Fireball_impact_burst`

Check the compiled emissive/opacity path, blend mode, unlit shading, and Niagara
particle-color/dynamic-parameter consumption. The required acceptance condition is a
fresh generator create followed by unchanged B10 thresholds producing
`warm_changed_pixels > 0`. WS-07 did not edit Material toolset sources.

This does not establish overall POC-B completion; metrics and bundle gates remain
separate.
