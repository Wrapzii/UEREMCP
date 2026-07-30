# WS-07 → WS-08: ribbon_trail inline MI create honesty

**Status:** Open — WS-07 mitigation landed; WS-08 durable fix still required  
**Owner:** WS-07 documents + Niagara pre-create cleanup; WS-08 owns `EnsureMasterMaterial` / `UeremcpMaterialService`

## Regression (orch `3093533`, log `20260730_060801`)

- B3/B5/B6/B8_save/B9 PASS; **B1/B4 FAIL** because `ribbon_trail` absent from merged manifest and renderer verify list.
- Other five roles create `MI_NS_POCB_Fireball_*` normally.

## Root cause (verified in editor logs)

Two interacting WS-08 issues, exposed honestly after **`6b1b4a0` CapPartial** (`failed_validation`, empty `PrimaryAsset` when no MI):

1. **Orphan / stale trail master package** under `/Game/__UeremcpPoc/Materials/Masters/M_Ueremcp_ProjTrail_*`:
   - `EnsureMasterMaterial` short-circuits when registry finds an existing master **without rebuilding the graph**.
   - When registry is out of sync (package “marked deleted” but `.uasset` remains), `CreateEmptyMaterial` fails with  
     `The asset 'M_Ueremcp_ProjTrail_*' already exists in package ... CanCreateAsset ... unattended ... false`.
   - FlowMap + master may persist; **`MI_NS_POCB_Fireball_ribbon_trail` never saves**.

2. **Prior graph wiring bug** (fixed in WS-08 `4f17911` on branch, must be on orch + **rebuilt `UeremcpMaterial` DLL**): `depth_fade` used pin `InOpacity` instead of `Opacity` → master graph incomplete → master-only partial.

FlowMap injection in WS-07 fixture / `PrepareInlineCreateSpec` is **not** the blocker when the above fire; trail fails before or without MI even with explicit `textures.FlowMap`.

## WS-07 mitigation (this branch)

Before inline `create_vfx_material`, when target MI is absent, delete co-located feature-signature master (registry asset or orphan `.uasset`) under allowed scratch roots. Prevents short-circuit / CreateAsset collision on repeated POC runs.

## WS-08 ask (durable)

1. **`EnsureMasterMaterial`:** On ensure, detect orphan packages (disk without loadable registry asset) and rebuild — mirror `ReleaseInProcessPackageForCreate` used for MI create.
2. **Do not short-circuit** reused masters without verifying wired features / graph revision (or bump signature when graph wiring changes).
3. Keep **`6b1b4a0` CapPartial honesty** — do not set `bSuccess=true` without loadable MI.

## WS-11 ask

`FireballInlineMaterials` / `run_poc_b_fireball.ps1` cleanup: delete `M_Ueremcp_ProjTrail_*` masters (and trail FlowMap textures) alongside MIs between runs — belt-and-suspenders.

## Verification

After WS-07 mitigation + fresh `UeremcpMaterial` + `UeremcpNiagara` build on RE orch:

`tests/run_poc_b_fireball.ps1` → all six roles in manifest; B4/B1 green. No POC-B claim until WS-11 harness PASS.
