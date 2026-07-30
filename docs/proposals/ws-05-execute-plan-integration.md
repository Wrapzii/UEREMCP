# WS-05 handoff: bind the execute_plan interpreter

**From:** WS-05 Protocol  
**To:** WS-15 Templates, WS-03 Core, domain workstreams, WS-01 orchestration  
**Status:** integration required; owned interpreter implemented

## Landed WS-05 surface

`FUeremcpPlanExecutor` now owns the accepted `execute_plan` interpretation:

- validates the request action and operation structure before mutation;
- rejects duplicate IDs, missing dependencies, cycles, recursive plans, missing
  semantic handlers, unsupported asset conditions, and missing atomic transaction
  coordination before dispatch;
- executes operations in stable topological order;
- resolves canonical object `$ref` and dollar-string references from completed
  operation responses;
- supports dependency skips, `operation_status` conditions, optional operations,
  and the three accepted `on_failure` policies;
- consolidates operation results, asset lists, changes, terminal validation and
  revision data, and summed `internal_operations`;
- preserves one `mcp_round_trips` count for internal template delegation;
- invokes cross-operation begin/commit/rollback callbacks and reports
  `rolled_back` only when the rollback callback confirms success.

The executor is deliberately independent of ToolsetRegistry and all domain modules.
This preserves the existing dependency direction and keeps Protocol testable outside
editor action registration.

## Required WS-15 binding

`UeremcpTemplates` already privately depends on `UeremcpProtocol`. Protocol cannot
depend back on Templates without a module cycle. In
`FUeremcpTemplatesModule::StartupModule`, after constructing the service, bind:

```cpp
UeremcpTemplates::SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest);
```

In `ShutdownModule`, call `ClearExecutePlanDelegate()` before destroying the service.
This is the only required Template-owned binding. No schema change is needed for the
delegate itself.

## Required Core/domain registration

Each executable goal-level action must register one
`FUeremcpPlanOperationHandler` during its owning module's startup and unregister it
during shutdown. The handler accepts a normal request envelope and returns the normal
response envelope; the executor does not invent a second domain protocol.

At minimum, the current elemental projectile template needs handlers for:

- `create_vfx_material` (WS-08);
- `create_niagara_effect` (WS-07).

WS-03 (or the settled editor integration owner) must register
`FUeremcpPlanTransactionCallbacks` backed by the accepted ADR-0005 FileSandbox plus
editor transaction scope. The callbacks must cover the whole plan, not one operation
at a time. Without all begin/commit/rollback callbacks, atomic template plans reject
before any domain action runs.

## Still schema-gated

The two WS-15 proposal gaps remain:

1. named modifiers have names but no executable delta in the frozen template schema;
2. `validation_rules` have no executable post-step representation in the batch plan
   schema.

The executor therefore does not claim those checks ran. WS-15's existing downgrade to
`partially_completed` remains correct until WS-01 supplies the frozen schema contract
and a domain validation handler is registered.

## Honest current status

The interpreter and its test delegates are executable. Runtime template
instantiation remains gated on the Template-owned delegate call, domain handler
registration, and the cross-operation transaction coordinator. Missing capabilities
fail before mutation; they do not return a validated status.
