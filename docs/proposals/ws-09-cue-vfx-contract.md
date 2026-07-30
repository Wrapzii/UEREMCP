# Proposal: Cue ↔ VFX contract (WS-09 ↔ WS-07) + montage note (WS-10)

- **From:** WS-09
- **To:** WS-07 (primary), WS-10 (FYI)
- **Date:** 2026-07-29
- **Status:** proposed
- **Evidence:** `docs/research/RB-12-gas-and-gameplay.md` §Q6–Q7

## Primary seam (RE-native)

RE presentation is **not** GameplayCue-driven. Prefer:

1. Niagara systems at stable paths (WS-07), materials (WS-08)
2. Optional `URESpellVFXDefinition` data asset with phase soft refs +
   color/scalar parameter maps `[VERIFIED: RESpellVFXDefinition.h]`
3. `FREAbilityDef` fields `CastNS` / `ProjectileNS` / `ImpactNS` / `VFXDefinition`
   / audio soft paths `[VERIFIED: REAbilityTypes.h:199-229]`

POC D batch under `/Game/__UeremcpTests/`:

| Phase | Suggested path pattern |
|---|---|
| Cast | `/Game/__UeremcpTests/VFX/NS_{id}_Cast` |
| Travel | `/Game/__UeremcpTests/VFX/NS_{id}_Travel` |
| Impact | `/Game/__UeremcpTests/VFX/NS_{id}_Impact` |
| VFX def | `/Game/__UeremcpTests/VFX/DA_{id}_SpellVFX` |

User parameters commonly pushed at runtime: Intensity, Width, Length, Velocity,
Lifetime, ImpactRadius, MasteryIntensity, color params
`[VERIFIED: RESpellVFXDefinition.h:58-61]`.

## Secondary seam (Epic GameplayCue)

`GameplayCueToolset.CreateCueNotifyAsset` creates an empty notify BP with tag on
CDO only — **no Niagara bind** `[VERIFIED: GameplayCueToolset.cpp:218-286]`.
Do not treat cue creation as "VFX done." If used later, WS-07 still must author
the notify graph/components; WS-09 only owns tag+notify scaffold coordination.

## Montage (WS-10)

`FREAbilityDef` has no cast-montage field today. Cast cosmetics are circle/Niagara
driven. POC D does **not** block on montage authoring. Future optional
`cast_montage` soft path can be added to the specification schema once WS-10
confirms a stable montage create/bind path.

## Ask

WS-07: confirm soft-path + `URESpellVFXDefinition` as the POC D handoff contract
(or counter-propose). WS-10: acknowledge montage is optional for D1–D8.

## Response (WS-01)

**Accepted as the RE-native presentation seam for POC D.** Epic GameplayCue is
secondary/scaffold-only. Montage is optional for D1–D8. WS-07/WS-10 may
counter-propose with evidence; until then, batch examples use soft-path Niagara
refs under `/Game/__UeremcpTests/`.
