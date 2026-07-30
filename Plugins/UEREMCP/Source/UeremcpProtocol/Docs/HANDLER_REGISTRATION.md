# Domain handler registration for execute_plan

**Owner:** WS-05 (API docs)  
**Runtime:** `FUeremcpPlanExecutor` in `UeremcpProtocol`  
**Template bind:** WS-15 owns
`UeremcpTemplates::SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest)`
on Templates module startup and `ClearExecutePlanDelegate()` on shutdown.
That bind is present on this synced branch. Domains must still register handlers;
the delegate does not register actions.

## Why domains register handlers

`execute_plan` does not call ToolsetRegistry or domain UFUNCTIONs directly. Each
goal-level `action` string must have a registered
`FUeremcpPlanOperationHandler`. Missing handlers reject **before** transaction
begin and before any mutation.

The Templates delegate only routes one `action: execute_plan` envelope into
`FUeremcpPlanExecutor::ExecuteRequest`. Domain actions inside the plan still
require `RegisterAction` in the owning module.

## Registration pattern (domain modules)

In your domain module `StartupModule` (or PostEngineInit, matching your toolset
registration order):

```cpp
#include "UeremcpPlanExecutor.h"

FString Error;
FUeremcpPlanExecutor::RegisterAction(
	TEXT("create_vfx_material"),
	[](const FString& RequestJson, FString& OutResponseJson, FString& OutError) -> bool
	{
		// Parse the normal request envelope. Call the same service the
		// AICallable tool uses. Return a normal response envelope JSON.
		OutResponseJson = /* domain service */ (RequestJson);
		OutError.Reset();
		return true; // false only when the handler itself could not run
	},
	Error);
```

On `ShutdownModule`:

```cpp
FUeremcpPlanExecutor::UnregisterAction(TEXT("create_vfx_material"));
```

Register after the domain service is ready; unregister before tearing it down.
Duplicate registration of the same action name fails closed.

### Contract

| Rule | Detail |
|---|---|
| Action name | `^[a-z][a-z0-9_]*$`, not `execute_plan` |
| Input | Full request envelope JSON (`schemas/envelope/request.schema.json`) |
| Output | Full response envelope JSON with valid `status`, `summary`, `metrics` |
| Nested timeout | Forced to `timeout_ms: 0` by the interpreter — handlers must complete inline |
| Success for `$ref` | Only `created_and_validated`, `modified_and_validated`, `created_with_warnings`, and `no_change_required` populate completed results |
| Dependents | Skip when a dependency is not in that success set (including optional failures) |
| Handler `bool` | `false` means the handler crashed/unavailable; prefer returning an envelope with `status: error` / `failed_validation` and `true` |
| Conditions | `operation_status` is evaluated; `asset_exists` / `asset_missing` reject at preflight until an evaluator exists |

## Minimum registrations for current templates

| Action | Owner |
|---|---|
| `create_vfx_material` | WS-08 |
| `create_niagara_effect` | WS-07 |
| Future GAS / BP / animation actions | domain WS |

Without these registrations, elemental template instantiate plans reject at
preflight even when the Templates delegate is bound.

## Transaction coordinator (not domain-owned)

Atomic plans require WS-03 (or settled integration owner) to register:

```cpp
FUeremcpPlanTransactionCallbacks Txn;
Txn.Begin = /* FileSandbox Enter + editor transaction */;
Txn.Commit = /* Persist + end transaction */;
Txn.Rollback = /* Discard + undo */;
FUeremcpPlanExecutor::SetTransactionCallbacks(MoveTemp(Txn), Error);
```

Without complete begin/commit/rollback, atomic `execute_plan` rejects before
dispatch. Non-atomic plans may run without callbacks. See
`docs/proposals/ws-05-execute-plan-integration.md`.

## Offline verification

Python mirror + tests:

```bash
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py/run_tests.py
```

Filter: `test_plan_executor.py`. C++ filter:
`UEREMCP.Protocol.PlanExecutor`.
