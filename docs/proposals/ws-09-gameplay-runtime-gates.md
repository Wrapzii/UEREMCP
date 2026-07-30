# Proposal: register Gameplay module and unblock guarded mutation

- **From:** WS-09
- **To:** WS-03 (plugin descriptor), WS-12 (mutator queue), WS-01 (coordination)
- **Date:** 2026-07-30
- **Status:** proposed
- **Blocks:** executable `create_spell` DataTable upsert and runtime tests

## Implemented owned slice

WS-09 now owns an independently testable `create_spell` preflight:

- strict `schemas/domains/gameplay/create_spell.schema.json`;
- deterministic semantic-to-`FREAbilityDef` row planning;
- RE Pattern B static validation;
- exact guarded DataTable write planning (package/object/row-struct identity and
  ordered acquire→sandbox→upsert→save→re-read→persist/discard steps);
- one envelope-shaped `AICallable` entry point;
- schema, local-header drift, and C++ automation tests;
- no Epic GAS/DataTable/Niagara/material primitives re-exposed.

The planner maps only fields read from the RE row definition
`[VERIFIED: REAbilityTypes.h:85-247]`. The tool returns
`partially_completed` and does not mutate assets while either gate below is open.
Dry-run preflight explicitly returns empty `changes`, null write-validation fields,
rollback unavailable/not performed, and a planning execution trace. It never emits
`created_and_validated` or `modified_and_validated`.

## Gate 1 — WS-03: module descriptor

`Plugins/UEREMCP/UEREMCP.uplugin` is WS-03-owned and does not list
`UeremcpGameplay`. UBT/UHT therefore cannot build or load the owned module from the
shipping plugin.

Requested descriptor entry:

```json
{
  "Name": "UeremcpGameplay",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": ["Editor"]
}
```

No new plugin dependency is required by the preflight slice. The module depends on
existing `ToolsetRegistry`, `UeremcpProtocol`, and `UeremcpSecurity` modules.

Acceptance evidence:

1. shipping plugin builds `UnrealEditor-UeremcpGameplay.dll`;
2. `UEREMCP.Gameplay.SpellPlanner.*` automation tests run;
3. runtime `list_toolsets` includes `UeremcpGameplay.UeremcpGameplayToolset`;
4. `describe_toolset` exposes only goal-level `CreateSpell`.

## Gate 2 — WS-12: mutator queue

ADR-0010 requires one active mutator per open project. The current public API says
`FUeremcpMutatorQueue::IsImplemented()` is false and `TryAcquire` is a stub
`[VERIFIED: UeremcpMutatorQueue.h:1-35]`.

WS-09 must not add a private lock or mutate around this shared policy. Requested
minimum API behavior:

1. FIFO game-thread acquire for write tier;
2. ownership keyed by `request_id`;
3. release by the same owner only;
4. queued/time-limited acquisition returns an ADR-0009 job handle;
5. tests for two concurrent writers and foreign release.

Once this lands, WS-09 will add the guarded operation:

1. acquire write slot;
2. enter the proven Content/ FileSandbox path;
3. create/load the test DataTable with row struct `/Script/RE.REAbilityDef`;
4. row-upsert only (never whole-table delete/recreate);
5. save;
6. re-read and compare every normalized row field;
7. persist or full-discard;
8. return `*_validated` only after successful re-read.

`UDataTable::AddRow` copies one row into the table
`[VERIFIED: Engine/DataTable.h:314-316; DataTable.cpp:519-555]`.
That is the intended semantic mutation substrate; generic DataTable CRUD remains
Epic-owned and is not exposed as a new primitive.

## Gate 3 — WS-01/WS-05: POC D example payload

`schemas/examples/batch-fireball-ability.json` now uses the accepted
`create_spell` action, but its specification predates the WS-09 domain contract.
It places `damage`, `damage_radius`, `projectile_effect`, `impact_effect`, and
`impact_status` in incompatible locations and omits required `element_color`.
The batch schema intentionally treats operation specifications as generic objects,
so global validation cannot detect this drift.

Requested update to the shared example:

- add `element_color`;
- keep physics under `delivery`;
- move damage/status fields under `impact`;
- move Niagara references under `presentation`;
- use `impact.status: "Burn"` (the verified RE enum), not `"Burning"`.

WS-09 will not edit the shared example. Acceptance is validating the embedded
`create_spell` specification against
`schemas/domains/gameplay/create_spell.schema.json` in CI.

## Remaining non-gates

- Multi-client replication proof remains WS-11/RB-14. Static Pattern B validation
  is not represented as net runtime validation.
- Production `DT_Abilities` mutation remains prohibited. The current tool accepts
  `/Game/__UeremcpTests/` targets only.
- Gameplay-tag INI mutation remains out of POC D by accepted WS-01 guidance.
