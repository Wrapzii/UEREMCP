# WS-15 handoff: register execute_plan domain handlers

- **From:** WS-15 Templates
- **To:** WS-07 Niagara, WS-08 Material, WS-03 Core
- **Status:** registration required
- **Blocks:** runtime execution of `niagara.projectile.elemental.v1`

## Why this is now the blocking edge

Templates binds `FUeremcpPlanExecutor::ExecuteRequest` during module startup and
clears it during shutdown. The executor snapshots registered handlers, confirms
every operation action has a handler, and requires transaction callbacks for an
atomic plan before beginning mutation
`[VERIFIED: UeremcpPlanExecutor.cpp:502-526]`.

The shipped elemental construction plan currently requires exactly:

| Action | Owner | Existing goal-level entry point |
|---|---|---|
| `create_vfx_material` | WS-08 | `UUeremcpMaterialToolset::CreateVfxMaterial(const FString&)` `[VERIFIED: UeremcpMaterialToolset.h:46-47]` |
| `create_niagara_effect` | WS-07 | `UUeremcpNiagaraToolset::CreateNiagaraEffect(const FString&)` `[VERIFIED: UeremcpNiagaraToolset.h:61-62]` |

## WS-07 / WS-08 registration contract

Each owning module registers its semantic action during startup:

```cpp
FString Error;
const bool bRegistered = FUeremcpPlanExecutor::RegisterAction(
    TEXT("create_vfx_material"), // WS-08; WS-07 uses create_niagara_effect
    [](const FString& RequestJson, FString& OutResponseJson, FString& OutError)
    {
        OutResponseJson = UUeremcpMaterialToolset::CreateVfxMaterial(RequestJson);
        if (OutResponseJson.IsEmpty())
        {
            OutError = TEXT("create_vfx_material returned an empty response");
            return false;
        }
        return true;
    },
    Error);
```

The executor registration API rejects invalid/recursive handlers and duplicate
action names `[VERIFIED: UeremcpPlanExecutor.cpp:383-402]`. Registration failure
must be logged and surfaced during integration; do not silently continue as if the
action were executable.

During module shutdown, unregister only the action owned by that module:

```cpp
FUeremcpPlanExecutor::UnregisterAction(TEXT("create_vfx_material"));
FUeremcpPlanExecutor::UnregisterAction(TEXT("create_niagara_effect"));
```

Each module calls only its own line above. `UnregisterAction` removes one named handler
`[VERIFIED: UeremcpPlanExecutor.cpp:405-409]`. Domain modules must not call
`ClearActionHandlers`, because that would remove other owners' registrations.

The adapter passes the nested request envelope through unchanged. The existing
goal-level tool remains responsible for parsing, security checks, mutation,
validation, and its normal response envelope. The executor validates that nested
responses contain a status and metrics before consolidation
`[VERIFIED: UeremcpPlanExecutor.cpp:603-620]`.

## WS-03 transaction callback contract

The elemental plan is atomic. WS-03 (or the settled integration owner) must call
`FUeremcpPlanExecutor::SetTransactionCallbacks` with all three callbacks:

- `Begin`
- `Commit`
- `Rollback`

Incomplete callback sets are rejected
`[VERIFIED: UeremcpPlanExecutor.cpp:417-430]`. The callbacks must coordinate the
accepted ADR-0005 transaction/sandbox boundary across the whole plan, not create
one independent transaction per nested operation.

Shutdown calls `FUeremcpPlanExecutor::ClearTransactionCallbacks`
`[VERIFIED: UeremcpPlanExecutor.cpp:433-436]`.

## Honest behavior until registration lands

- Missing `create_vfx_material` or `create_niagara_effect` rejects before mutation
  with `no handler registered for '<action>'`
  `[VERIFIED: UeremcpPlanExecutor.cpp:509-518]`.
- Missing atomic callbacks rejects before mutation with
  `atomic execute_plan requires transaction callbacks`
  `[VERIFIED: UeremcpPlanExecutor.cpp:520-526]`.
- Once handlers execute successfully, Templates still downgrades a validated
  domain result to `partially_completed` while template `validation_rules` have no
  executable post-step contract.

## Offline drift guard

WS-15 tests derive the unique action set from every shipped
`construction_plan` and require this handoff to name each action and owner. Adding
a new plan action without a registration handoff is therefore an offline test
failure rather than a runtime surprise.
