# WS-11 B10 particle-observation root cause

## Scope

This change repairs only the rendered B10 validation harness. It does not modify
Niagara creation sources, soften the visible-fire thresholds, or claim overall
POC-B completion.

## Root cause

B10 relied on a realtime editor viewport to advance Niagara between latent command
updates. That path ticks the editor world with `LEVELTICK_ViewportsOnly`, but the
fresh production system remained pending with six runtime emitter instances and
zero spawned particles. The WS-07 runtime probe instead performs one
`LEVELTICK_All` world tick and then advances the forced-solo component explicitly.
[VERIFIED: Engine/Source/Runtime/Engine/Private/LevelTick.cpp:1613-1616]
[VERIFIED: NiagaraSystemSimulation.cpp:1425-1502]
[VERIFIED-RUNTIME: pre-fix rendered B10
`editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_091504.log` reported
`runtime_emitter_instances=6`, `total_spawned_particles=0`, and `particle_count=0`]

Two observation defects compounded the tick defect:

1. The registered `ANiagaraActor` component defaults to auto-activate, so B10's
   former `SetAsset` call activated an intermediate non-solo instance before
   `SetForceSolo`. The harness now disables auto-activation and deactivates before
   assigning the asset. [VERIFIED: NiagaraComponent.cpp:685,4620-4694]
2. B10 read only live particles at the final instant without finalizing a possible
   concurrent Niagara tick. It now waits for finalization and records maximum live
   particles, cumulative spawned particles, and emitter-instance count throughout
   warm-up. [VERIFIED:
   Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraSystemInstanceController.h:153]
   [VERIFIED:
   Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraEmitterInstance.h:71-73]

The harness now matches the proven activation sequence: configure a non-auto,
forced-solo component; activate; issue one full world tick; explicitly advance the
solo simulation on each latent warm-up update; synchronize before observation.
[VERIFIED-RUNTIME: production B10
`editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_091657.log` reported
`particle_count=422`, `total_spawned_particles=715`, and
`runtime_emitter_instances=6`]

## Rendered results

Production `/Game/__UeremcpPoc/NS_POCB_Fireball`:

```text
UEREMCP_POC_B10_EVIDENCE={"status":"fail","changed_pixels":5405,
"warm_changed_pixels":0,"particle_count":422,"total_spawned_particles":715,
"runtime_emitter_instances":6,...}
UEREMCP_POC_B10_OUTCOME=FAIL reason=visible_fire_signature_not_observed
```

Screenshot: `tests/integration/_artifacts/poc_b10_fireball.png`.
[VERIFIED-RUNTIME:
`tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_091657.log`]

Known-good canary `/Game/RE/City/FX/NS_CityBrazierFlame`:

```text
UEREMCP_POC_B10_EVIDENCE={"status":"pass","changed_pixels":5407,
"warm_changed_pixels":43,"particle_count":50,"total_spawned_particles":444,
"runtime_emitter_instances":1,...}
UEREMCP_POC_B10_OUTCOME=PASS proof=viewport_pixel_delta_with_fire_signature
```

Screenshot: `tests/integration/_artifacts/poc_b10_canary.png`.
[VERIFIED-RUNTIME:
`tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_091740.log`]

## Remaining gaps

The particle-observation regression is closed. Production B10 still honestly fails
because no warm pixels are visible at the unchanged threshold of 20. Color/material
visibility returns to WS-07/WS-08. Metrics and bundle acceptance remain open, so this
is not an overall POC-B claim.
