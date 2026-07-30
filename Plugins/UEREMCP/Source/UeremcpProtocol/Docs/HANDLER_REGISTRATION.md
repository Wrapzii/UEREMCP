# Domain handler registration for execute_plan

**Owner:** WS-05 (API docs)  
**Runtime:** `FUeremcpPlanExecutor` in `UeremcpProtocol`  
**Binding:** WS-15 calls `UeremcpTemplates::SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest)`

## Why domains register handlers

`execute_plan` does not call ToolsetRegistry or domain UFUNCTIONs directly. Each
goal-level `action` string must have a registered
`FUeremcpPlanOperationHandler`. Missing handlers reject **before** transaction
begin and before any mutation.

## Registration pattern (domain modules)

In your domain module `StartupModule` (or PostEngineInit, matching your toolset
registration order):

```cpp
FString Error;
FUeremcpPlanExecutor::RegisterAction(
    TEXT("create_vfx_material"),
    [](const FString& RequestJson, FString& OutResponseJson, FString& OutError) -> bool
    {
        // Parse the normal request envelope. Call the same service the
        // AICallable tool uses. Return a normal response envelope JSON.
        OutResponseJson = UUeremcpMaterialToolset::CreateVfxMaterial(RequestJson);
        OutError.Reset();
        return true; // false only when the handler itself could not run
    },
    Error);
```

On `ShutdownModule`:

```cpp
FUeremcpPlanExecutor::UnregisterAction(TEXT("create_vfx_material"));
```

### Contract

| Rule | Detail |
|---|---|
| Action name | `^[a-z][a-z0-9_]*$`, not `execute_plan` |
| Input | Full request envelope JSON (`schemas/envelope/request.schema.json`) |
| Output | Full response envelope JSON with valid `status`, `summary`, `metrics` |
| Nested timeout | Forced to `timeout_ms: 0` by the interpreter — handlers must complete inline |
| Success for `$ref` | Only `*_validated` / `no_change_required` populate completed results |
| Handler `bool` | `false` means the handler crashed/unavailable; prefer returning an envelope with `status: error` / `failed_validation` and `true` |

## Minimum registrations for current templates

| Action | Owner |
|---|---|
| `create_vfx_material` | WS-08 |
| `create_niagara_effect` | WS-07 |
| Future GAS / BP / animation actions | domain WS |

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
dispatch. See `docs/proposals/ws-05-execute-plan-integration.md`.

## Offline verification

Python mirror + tests:

```bash
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py/run_tests.py
```

Filter: `test_plan_executor.py`. C++ filter:
`UEREMCP.Protocol.PlanExecutor`.
