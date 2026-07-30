# WS-07 → WS-08: ribbon_trail inline MI create honesty

**Status:** Resolved — WS-08 `2187d69`; WS-07 mitigation `ee905ed` (orch `886d09d`) retained  
**Owner:** WS-07 documents + Niagara pre-create cleanup; WS-08 owns `EnsureMasterMaterial` / `UeremcpMaterialService`

## Resolution (2026-07-30)

WS-08 **`2187d69`**: MainTexture UV pin **`UVs`** (not `Coordinates`).  
`UeremcpMaterial.Toolset.CreateVfxMaterial.FireballRibbonTrailPoc` **PASS**.

Prior WS-08 fixes on the same lineage (`4f17911` depth_fade `Opacity` pin, panning Speed float2) remain prerequisites; the remaining gap was the UV pin name.

**Pending:** Full fireball re-run (`tests/run_poc_b_fireball.ps1`) after orch lands `2187d69` + rebuilt `UeremcpMaterial` DLL. No POC-B claim until WS-11 harness PASS.

## Regression (orch `3093533`, log `20260730_060801`)

- B3/B5/B6/B8_save/B9 PASS; **B1/B4 FAIL** because `ribbon_trail` absent from merged manifest and renderer verify list.
- Other five roles create `MI_NS_POCB_Fireball_*` normally.

## Root cause (verified in editor logs)

Three WS-08 graph / ensure issues, exposed honestly after **`6b1b4a0` CapPartial** (`failed_validation`, empty `PrimaryAsset` when no MI):

1. **Orphan / stale trail master package** under `/Game/__UeremcpPoc/Materials/Masters/M_Ueremcp_ProjTrail_*`:
   - `EnsureMasterMaterial` short-circuits when registry finds an existing master **without rebuilding the graph**.
   - When registry is out of sync (package “marked deleted” but `.uasset` remains), `CreateEmptyMaterial` fails with  
     `The asset 'M_Ueremcp_ProjTrail_*' already exists in package ... CanCreateAsset ... unattended ... false`.
   - FlowMap + master may persist; **`MI_NS_POCB_Fireball_ribbon_trail` never saves**.

2. **Graph wiring bugs** (fixed on WS-08 branch, must be on orch + **rebuilt `UeremcpMaterial` DLL**):
   - `4f17911`: `depth_fade` pin `Opacity` (not `InOpacity`); panning Speed float2 wiring.
   - **`2187d69`**: MainTexture UV pin `UVs` (not `Coordinates`) → master graph incomplete until fixed.

FlowMap injection in WS-07 fixture / `PrepareInlineCreateSpec` is **not** the blocker when the above fire; trail fails before or without MI even with explicit `textures.FlowMap`.

## WS-07 mitigation (landed `ee905ed`)

Before inline `create_vfx_material`, when target MI is absent, delete co-located feature-signature master (registry asset or orphan `.uasset`) under allowed scratch roots. Prevents short-circuit / CreateAsset collision on repeated POC runs. Retained as belt-and-suspenders alongside WS-08 graph fixes.

## WS-08 durable items (optional follow-up)

1. **`EnsureMasterMaterial`:** On ensure, detect orphan packages (disk without loadable registry asset) and rebuild — mirror `ReleaseInProcessPackageForCreate` used for MI create.
2. **Do not short-circuit** reused masters without verifying wired features / graph revision (or bump signature when graph wiring changes).
3. Keep **`6b1b4a0` CapPartial honesty** — do not set `bSuccess=true` without loadable MI.

Graph wiring for ribbon trail POC is fixed; orphan-master hardening remains optional hardening.

## WS-11 ask

`FireballInlineMaterials` / `run_poc_b_fireball.ps1` cleanup: delete `M_Ueremcp_ProjTrail_*` masters (and trail FlowMap textures) alongside MIs between runs — belt-and-suspenders.

## Verification

After orch lands **`2187d69`** + fresh `UeremcpMaterial` + `UeremcpNiagara` build on RE:

1. `UeremcpMaterial.Toolset.CreateVfxMaterial.FireballRibbonTrailPoc` — **PASS** (confirmed WS-08).
2. `tests/run_poc_b_fireball.ps1` → all six roles in manifest; B1/B4 green — **pending orch re-run**.
