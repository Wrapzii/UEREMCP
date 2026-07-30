# Gameplay domain schemas

Owner: WS-09.

`create_spell.schema.json` is the RE-native semantic contract for POC D. It maps
to one `FREAbilityDef` row consumed by `CastAbility` / `AuthorityCastAbility`; it
does not create disconnected Epic GAS assets
`[VERIFIED: REAbilityTypes.h:85-247; REPlayerVisualCombatComponent.cpp:3912-3984]`.

The envelope target is the DataTable package path. `row_name` is both the row key
and `AbilityId`; repeated requests never auto-suffix either identity (ADR-0006).
POC paths are restricted to `/Game/__UeremcpTests/` until production-table policy
is explicitly approved.

Presentation fields are soft references to WS-07/WS-08 outputs. The schema does
not recreate Niagara or material primitives. The networking block declares the
existing RE Pattern B contract; it does not generate RPCs or claim multi-client
runtime proof.

## Fixtures

| File | Purpose |
|---|---|
| `fixtures/poc_d_execute_plan_create_spell.json` | One-op `execute_plan` → `create_spell` (schema-valid) |
| `fixtures/poc_d_batched_spell_plan.json` | Material → Niagara → `create_spell` with `depends_on` / `$ref` |
| `golden/dry_run_preflight.response.json` | Honest dry-run envelope |

## Implementation boundary (2026-07-30)

- deterministic specification-to-row planning and Pattern B static checks: **implemented**
- sandboxed DataTable upsert, save, re-read, persist/discard: **implemented** via
  `FUeremcpAbilityTableMutator` under `FUeremcpMutatingDispatch`
- `create_spell` registered with `FUeremcpPlanExecutor` for `execute_plan`: **implemented**
- production `DT_Abilities` mutation: **prohibited** (scratch root only)
- listen-server / multi-client replication proof: **WS-11 / RB-14** (not claimed here)
- shared `schemas/examples/batch-fireball-ability.json` still drifts from this schema
  (WS-01-owned); WS-09 fixtures are the conforming POC D payloads
