# WS-14 → WS-07: fix mesh renderer binding reread key

**Status:** Blocking successful primitive-baseline measurement  
**Fixture:** `schemas/domains/niagara/fixtures/poc_b_primitive_baseline.py`  
**Measured artifact:** `docs/reviews/metrics/artifacts/poc_b_primitive_baseline_post_04b3541_failed_20260730.json`

Commit `04b3541` successfully removes the inherited `Minimal` emitter before adding
the six role emitters. Three independent clean trials then progressed to 42 primitive
operations each, but all failed in `binding_path` with `KeyError('ExplicitMat')`.

The live `GetRendererData` payload for `FlameShell` serializes
`OverrideMaterials[0].explicitMat.refPath` (lower-camel), while the fixture rereads
`OverrideMaterials[0]["ExplicitMat"]["refPath"]`. [VERIFIED-RUNTIME:
`NiagaraToolsets.NiagaraToolset_System.GetRendererData` on
`/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline`, 2026-07-30]

WS-07 should update both the mesh patch and reread paths to use the live lower-camel
field name, add a fixture test for that payload shape, and hand the corrected fixture
back for at least one clean live trial (preferably three). Until then, WS-14 records
the measured operation counts and wall clocks only as failed trials and claims no
reduction ratio or overall POC-B result.
