# Proposal: unblock guarded Gameplay mutation

- **From:** WS-09
- **To:** WS-01/WS-05 (shared example)
- **Date:** 2026-07-30
- **Status:** partially resolved
- **Blocks:** executable `create_spell` DataTable upsert and runtime tests

## Implemented owned slice

WS-09 now owns an independently testable `create_spell` preflight:

- strict `schemas/domains/gameplay/create_spell.schema.json`;
- deterministic semantic-to-`FREAbilityDef` row planning;
- RE Pattern B static validation;
- exact guarded DataTable write planning (request ownership, envelope controls,
  package/object/row-struct identity, revision/idempotency checks, and ordered
  acquire→sandbox→upsert→save→re-read→persist/discard steps);
- one envelope-shaped `AICallable` entry point;
- schema, local-header drift, and C++ automation tests;
- no Epic GAS/DataTable/Niagara/material primitives re-exposed.

The planner maps only fields read from the RE row definition
`[VERIFIED: REAbilityTypes.h:85-247]`. The tool returns
`partially_completed` and does not mutate assets while the owned DataTable executor
remains incomplete.
Dry-run preflight explicitly returns empty `changes`, null write-validation fields,
rollback unavailable/not performed, and a planning execution trace. It never emits
`created_and_validated` or `modified_and_validated`.

## Resolved — module registration

WS-03 registered `UeremcpGameplay` in the plugin descriptor on orchestration commit
`807d4f9`. WS-09 already registers `UUeremcpGameplayToolset` on `PostEngineInit` and
unregisters the delegate during shutdown, so no additional owned C-1 change is
needed `[VERIFIED: Plugins/UEREMCP/Source/UeremcpGameplay/Private/UeremcpGameplayModule.cpp:17-27,38]`.

## Resolved — WS-12 mutator queue

WS-12 implemented per-project FIFO acquisition, owner-only release, stable waiter
job IDs, cancellation, and append-only audit on commit `1fd0eef`
`[VERIFIED: 1fd0eef:Plugins/UEREMCP/Source/UeremcpSecurity/Public/UeremcpMutatorQueue.h;
1fd0eef:Plugins/UEREMCP/Source/UeremcpSecurity/Public/UeremcpAuditLog.h]`.

WS-09 consumes this queue only through `FUeremcpMutatingDispatch`; it does not fork
acquisition, waiter polling, audit, or release policy. Acquisition failure is
fail-closed, and queued requests return Core's stable ADR-0009 job response.
`dry_run` does not mutate.

## Resolved — WS-03 Core dispatcher

WS-03 landed `FUeremcpMutatingDispatch` on orchestration commit `5b728ef`. It
preserves explicit destructive-dry-run intent, applies permission and path policy,
returns queued ADR-0009 job responses, holds the mutator through terminal audit, and
releases by RAII/`Complete`
`[VERIFIED: 5b728ef:Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpMutatingDispatch.h;
5b728ef:Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpMutatingDispatch.cpp]`.

WS-09 non-dry `create_spell` now calls `TryBegin` before domain mutation and returns
the blocking response unchanged when permission, path, or queue admission fails.
Every admitted terminal response is returned through `Complete`; Gameplay no longer
forks queue or audit logic. Dry-run remains a no-mutation planning path.

## Remaining owned implementation residual

No upstream runtime gate remains. The following WS-09-owned DataTable executor work
is still required before non-dry requests may report mutation:

1. enter the proven Content/ FileSandbox path while the dispatcher holds the slot;
2. create/load the test DataTable with row struct `/Script/RE.REAbilityDef`;
3. row-upsert only (never whole-table delete/recreate);
4. save;
5. re-read and compare every normalized row field;
6. persist or full-discard;
7. populate the terminal response and call dispatcher `Complete`;
8. return `*_validated` only after successful re-read.

The prepared plan captures `request_id` for queue ownership, mode, `dry_run`,
atomicity, save, validation, rollback, queue timeout, revision-conflict policy,
optional `expected_revision`, and optional idempotency key. A plan can only become
eligible for a validated mutation status when it is non-dry, saved, validated, and
successfully re-read; the current preflight never makes that claim.

`UDataTable::AddRow` copies one row into the table
`[VERIFIED: Engine/DataTable.h:314-316; DataTable.cpp:519-555]`.
That is the intended semantic mutation substrate; generic DataTable CRUD remains
Epic-owned and is not exposed as a new primitive.

## Shared-contract follow-up — WS-01/WS-05: POC D example payload

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

## Integration/compile residual outside WS-09

The WS-09 branch intentionally does not copy WS-12/WS-03-owned files; compilation of
the dispatcher path therefore requires integration of `1fd0eef` and `5b728ef`.
An earlier integrated build also stopped before compiling Gameplay because
`UeremcpMaterial.Build.cs` referenced an unresolved `Editor` module
`[VERIFIED-RUNTIME: Build.bat reported "Could not find definition for module
'Editor', referenced via REEditor -> UeremcpMaterial.Build.cs"]`. WS-09 does not own
that module. The latest retry only waited on the global UBT mutex and exited
`0xFFFFFFFF`, so it did not supersede that evidence. These are C++ test-execution
blockers, not Gameplay mutation design gates, and must be rechecked on the
orchestration lane.

## Remaining non-gates

- Multi-client replication proof remains WS-11/RB-14. Static Pattern B validation
  is not represented as net runtime validation.
- Production `DT_Abilities` mutation remains prohibited. The current tool accepts
  `/Game/__UeremcpTests/` targets only.
- Gameplay-tag INI mutation remains out of POC D by accepted WS-01 guidance.
