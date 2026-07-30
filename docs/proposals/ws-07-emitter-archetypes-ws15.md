# WS-07 → WS-15: Niagara emitter archetypes for template library

- **From:** WS-07
- **To:** WS-15
- **Date:** 2026-07-29
- **Status:** research handoff (implementation gated on Phase 1)

## Ask

Seed the template library with six emitter archetypes that compose POC B/C
categories (projectile fireball → ice variation and beyond). Do not invent new
Epic primitives — instantiate from engine/project templates and record
`construction_plan` + user-param contracts.

## Archetypes

| ID | Role in fireball | Suggested base | User params to expose |
|---|---|---|---|
| `niagara.emitter.core` | core / REF glow | Minimal / SingleLoopingParticle | Color, Scale, Intensity |
| `niagara.emitter.shell` | flame_shell / mesh spiral | UpwardMeshBurst / mesh sprite | Color, Scale |
| `niagara.emitter.sparks` | sparks | SimpleSpriteBurst | Color, Scale, Velocity |
| `niagara.emitter.smoke` | smoke | Fountain / BlowingParticles | Color, Scale, Shape, Velocity |
| `niagara.emitter.ribbon_trail` | ribbon_trail | LocationBasedRibbon | Color, Scale |
| `niagara.emitter.impact_burst` | impact_burst | OmnidirectionalBurst | Color, Scale, Intensity |

## Prior-art pattern (promote, don't rebuild)

`/Game/VFX/Spells/Firebolt/Systems/NS_FB_Projectile` already has six emitters
(Sparks1, Smoke, Spiral1, Flash, Trail, REF) and 18 User color/scale/velocity
parameters — verified via NiagaraToolsets GetSystemSummary
`[VERIFIED-RUNTIME: WS-07 2026-07-29]`. Treat this as the projectile pattern
source for `niagara.projectile.fireball.v1`.

## Elemental variation contract

For fire/water/wind/earth/ice templates, vary:

1. User LinearColor defaults
2. Material bindings (coordinate with WS-08)
3. Optional force modules (noise vs gravity vs vortex) — composition only; no new module scripts

## Out of scope for templates

- Authoring new Niagara module script graphs
- Event-handler stack fidelity until Epic topology exposes them (see RB-07)

## Response (WS-01)

**Accepted as WS-15 research seed** (implementation gated). Aligns with ADR-0008
parameterized elemental templates + Firebolt prior art. WS-15: record archetypes
in template library design; do not invent new Niagara module scripts.
