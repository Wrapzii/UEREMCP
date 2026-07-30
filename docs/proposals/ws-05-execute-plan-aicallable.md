# WS-05 handoff: expose Core execute_plan AICallable

**From:** WS-05  
**To:** WS-03 (Core), WS-01 (catalog), WS-15 (Templates already delegates internally)  
**Status:** protocol adapter implemented; AICallable wrapper unowned

## Verified gap

- `FUeremcpPlanExecutor` interprets `action: execute_plan` and Templates binds
  `SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest)`.
- No agent-facing `AICallable` tool named `ExecutePlan` / `execute_plan` exists on
  `UUeremcpReferenceToolset` (or any other Core toolset) in this tree.
- `docs/CAPABILITY_CATALOG.md` still lists `execute_plan` as `planned`.

Protocol cannot register ToolsetRegistry tools
`[VERIFIED: UeremcpProtocol.Build.cs — deliberately ToolsetRegistry-free]`.

## Landed WS-05 adapter

`FUeremcpPlanActions::ExecutePlan(RequestJson)` in `UeremcpProtocol`:

- parses the frozen request envelope; requires `action: execute_plan`
- replays `idempotency_key` via `FUeremcpIdempotencyStore` (ADR-0006)
- dispatches through `FUeremcpPlanExecutor` (one plan in → consolidated envelope out)
- preserves dry_run / options on the request JSON for nested ops
- `timeout_ms == 0` → inline; `timeout_ms > 0` → job handle on forced/partial path
  (ADR-0009); synchronous finish-within-timeout when no force probe is set
- honest statuses only (`created_and_validated`, `rolled_back`,
  `partially_completed`, `rejected`, …)

Automation filter prefix: `UEREMCP.Protocol.PlanActions`.

## Exact WS-03 wrapper

Add to `UUeremcpReferenceToolset` (same pattern as `GetJobResult` /
`CancelJob` in commit `2c35730`):

```cpp
/**
 * Execute a complete multi-operation plan (action=execute_plan).
 * Delegates to FUeremcpPlanActions — no additional parsing.
 *
 * @param RequestJson Request envelope JSON (schemas/batch/plan.schema.json
 *        as specification).
 * @return Response envelope JSON with consolidated result + change manifest.
 */
UFUNCTION(meta = (AICallable), Category = "UEREMCP")
static FString ExecutePlan(const FString& RequestJson);
```

```cpp
#include "UeremcpPlanActions.h"

FString UUeremcpReferenceToolset::ExecutePlan(const FString& RequestJson)
{
	return FUeremcpPlanActions::ExecutePlan(RequestJson);
}
```

Registration is already performed for `UUeremcpReferenceToolset` via
`UToolsetRegistry::RegisterToolsetClass` on PostEngineInit
`[VERIFIED: UeremcpCoreModule.cpp — RegisterToolsetClass on GetOnPostEngineInit]`.
Adding the UFUNCTION is sufficient; no second toolset class is required.

Core automation should call the UFUNCTION directly (CDO) and assert:

1. success with a registered fake/domain handler returns validated status
2. handler failure yields `rolled_back` when atomic + rollback_on_failure
3. `timeout_ms > 0` with unfinished work returns `partially_completed` + job

## WS-01 catalog

Flip `execute_plan` from `planned` → `partial` once the Core wrapper lands
(adapter + tests exist; AICallable registration is the remaining gate).
Specification schema remains `schemas/batch/plan.schema.json` (owned by WS-05;
already frozen). Until then, catalog stays `planned` — do not claim agent-facing
reachability from MCP.

## Explicit non-goals for WS-05

- No ToolsetRegistry dependency on `UeremcpProtocol`
- No second plan interpreter (Templates continues to delegate to the same executor)
- No claim of overall POC-B

## Orch integration run (isolated worktree)

This worktree does not share the RE project junction with `UEREMCP-ws01`. After
WS-03 lands the wrapper, orch should:

1. Build editor with UEREMCP enabled against RE
2. Confirm MCP `list_toolsets` / tool schema includes `ExecutePlan`
3. Dry-run a one-op plan against a registered domain handler
4. Confirm Templates `instantiate_template` still uses the same executor path
