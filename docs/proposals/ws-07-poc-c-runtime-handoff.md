# POC C ice variation runtime handoff

- **Owners:** WS-07 + WS-15
- **Branch:** `ws-07-poc-c-ice-variation`
- **Base:** `5235698ad76f2d7fd4e69c4abddf9842151ae4ea`
- **Status:** implementation and offline contracts green; RE runtime proof blocked by
  another process holding the editor. This document does **not** claim POC C.

## Audit disposition

No new primitive or parallel protocol was added.

- Epic Niagara exposes system creation, emitter addition, user-variable mutation, and
  system summary APIs `[VERIFIED:
  $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:1308,1358]`.
- `AddUserVariable` writes through the exposed-parameter store, so it can update the
  variation's existing `User.*` values rather than requiring a second primitive
  `[VERIFIED:
  $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/NiagaraExternalSystemEditorUtilities.cpp:1897-1925]`.
- Existing REAgentTools Niagara authoring remains absent and delegates to Epic batching
  `[VERIFIED: docs/audit/reagenttools.md:140,213]`.

The implementation therefore extends the existing `create_niagara_effect` composition
path and ADR-0008 `instantiate_template` → `execute_plan` path.

## Implemented proof surface

- `base_system` now duplicates an existing Niagara system and preserves its inherited
  emitters; only requested variation emitters are added.
- Ice inputs materialize the ice material/parameter preset.
- `crystalline_fragments` adds `Crystalline` and a changed `IceImpact` burst.
- Renderer material roles can bind to inherited emitters, and particle colour changes
  apply to inherited plus added emitters.
- Responses emit stable `inherited:*` and `overridden:*` interpretation notes and list
  the source Niagara system in `result.reused_assets`.
- The same `niagara.projectile.elemental.v1` template materializes an ice generation
  from the fireball and a wind third generation from the ice result.

## Current binary status

| Criterion | Status | Current evidence |
|---|---|---|
| C1 one request | skip | canonical single-call fixture exists; live MCP not run |
| C2 inherited vs overridden | pass offline / skip runtime | template tests assert response provenance; RE filter pending |
| C3 ice materials/parameters | pass offline / skip runtime | exact ice preset asserted; editor mutation pending |
| C4 crystalline component | pass offline / skip runtime | materialized role asserted; editor re-read filter pending |
| C5 networking + damage unchanged (acceptance file) | fail | Niagara source has no verified networking/damage contract; no claim is made |
| C6 reusable conforming template | pass offline / skip runtime | schema validation green; live instance pending |
| C7 third generation | pass offline / skip runtime | same template id and changed source/element/target asserted; live run pending |

The user-level “impact changed” and “shared VFX behaviour preserved” checks have
executable editor assertions, but are not substitutes for the acceptance file's C5
networking/damage requirement.

## Exact orch commands

Precondition: the editor owner closes PID holding RE, and orchestration retargets the
RE `Plugins/UEREMCP` junction to the integrated POC C tip. Do not retarget it from this
branch while another editor owner is active.

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Niagara.Create.PocCVariationRuntime"
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Templates.POCC.ThirdGeneration"
```

After both filters pass, run the canonical requests in
`schemas/domains/niagara/fixtures/poc_c_ice_variation_request.json` through MCP
`call_tool`, persist the raw envelopes under `tests/integration/_logs/`, and have
WS-11/WS-01 evaluate C1–C7. Do not convert offline materialization into a live claim.

