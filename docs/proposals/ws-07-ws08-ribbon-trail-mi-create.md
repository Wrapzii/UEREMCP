# WS-07 → WS-08: ribbon_trail inline MI create honesty

**Status:** Open (WS-08 follow-up if trail MI still absent after WS-07 `PrepareInlineCreateSpec` fix)  
**Owner:** WS-07 documents; WS-08 owns `UeremcpMaterialService`

## Symptom (POC-B fireball, orch `279f09a`)

- Five of six material roles resolved and B4 re-read verified.
- `ribbon_trail`: no `MI_NS_POCB_Fireball_ribbon_trail` on disk; trail **master** persisted under `/Game/__UeremcpPoc/Materials/Masters/`.
- Aggregate `B4_material_bindings_verified` / `validation.material_bindings_verified` reported **true** (WS-07 bug: denominator was resolved roles, not requested roles).

## WS-07 root causes addressed in this branch

1. **B4 honesty:** `bAllRequestedVerified` now compares `VerifiedRoles.Num()` to `Requests.Num()` (all parsed `specification.materials` roles), not `RoleToCanonicalMaterialPath.Num()`.
2. **Trail create_spec contract:** Fireball fixture and `BuildDefaultFireballMaterialCreateSpec` omitted `textures.FlowMap` while requesting `panning_textures`. WS-08 passing trail test (`UeremcpMaterialToolsetTests.cpp` ~474–488) includes explicit FlowMap generation. WS-07 `PrepareInlineCreateSpec` merges the same default when purpose is `elemental_projectile_trail` and `panning_textures` is present.

## WS-08 issue if MI still missing after texture merge

`CapPartialWhenProofUnavailable` in `UeremcpMaterialService.cpp` sets `bSuccess = true` and `PrimaryAsset = TargetPath` when MI creation, save, compile, or registry reload fails, while `TryPersistVfxAssets` may still save the **master** only. WS-07 correctly rejects that path via `VerifyPrimaryAssetIsMaterialInterface`, but the material service should not report `bSuccess = true` when no loadable MI exists on disk.

**Ask WS-08:** Return `bSuccess = false` (or omit `PrimaryAsset`) when `CapPartialWhenProofUnavailable` is invoked because MI creation/save failed, so inline create failures are not masked as success upstream.

## Verification

Re-run `UEREMCP.Niagara.POCB.FireballInlineMaterials` on RE orch after build. B4 must stay **false** until all six roles resolve **and** re-read verify; no POC-B claim until harness PASS.
