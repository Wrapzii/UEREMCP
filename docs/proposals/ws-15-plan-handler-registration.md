# WS-15 handoff: register execute_plan domain handlers

- **From:** WS-15 Templates
- **To:** WS-07 Niagara, WS-08 Material, WS-03 Core
- **Status:** handlers and transaction callbacks landed on orch
- **Residual:** executable template validation rules and modifiers

## Landed execution edge

Templates binds `FUeremcpPlanExecutor::ExecuteRequest` during module startup and
clears it during shutdown. The executor snapshots registered handlers, confirms
every operation action has a handler, and requires transaction callbacks for an
atomic plan before beginning mutation
`[VERIFIED: UeremcpPlanExecutor.cpp:502-526]`.

The shipped elemental construction plan currently requires exactly:

| Action | Owner | Registered adapter |
|---|---|---|
| `create_vfx_material` | WS-08 | `FUeremcpMaterialPlanHandlers::Register` delegates to `UUeremcpMaterialToolset::CreateVfxMaterial` `[VERIFIED: UeremcpMaterialPlanHandlers.cpp:10-22,31-41]` |
| `create_niagara_effect` | WS-07 | `FUeremcpNiagaraPlanHandlers::Register` delegates to `UUeremcpNiagaraToolset::CreateNiagaraEffect` `[VERIFIED: UeremcpNiagaraPlanHandlers.cpp:10-22,31-41]` |

Both owning modules invoke registration during startup
`[VERIFIED: UeremcpMaterialModule.cpp:56-64; UeremcpNiagaraModule.cpp:56-64]`.

## WS-07 / WS-08 registration contract (landed)

The landed adapters follow this semantic shape:

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

During module shutdown, each owner unregisters only its own action:

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

## WS-03 transaction callback contract (landed)

The elemental plan is atomic. WS-03 now registers all three callbacks:

- `Begin`
- `Commit`
- `Rollback`

Incomplete callback sets are rejected
`[VERIFIED: UeremcpPlanExecutor.cpp:417-430]`. Core wires `Begin`, `Commit`, and
`Rollback` to one plan transaction coordinator
`[VERIFIED: UeremcpPlanTransactionCoordinator.cpp:35-42]`; module startup and
shutdown register/clear it
`[VERIFIED: UeremcpCoreModule.cpp:38,70-78]`.

Shutdown calls `FUeremcpPlanExecutor::ClearTransactionCallbacks`
`[VERIFIED: UeremcpPlanExecutor.cpp:433-436]`.

## Honest residual behavior

- Registration is present, but the executor still rejects before mutation if a
  module failed to load or register (`no handler registered for '<action>'`)
  `[VERIFIED: UeremcpPlanExecutor.cpp:509-518]`.
- It likewise rejects if atomic callbacks are unavailable
  `[VERIFIED: UeremcpPlanExecutor.cpp:520-526]`.
- After successful domain execution, Templates still downgrades a validated result
  to `partially_completed` while template `validation_rules` have no executable
  post-step contract.
- WS-05 commit `1ef125d` proposes deterministic `modifier_definitions` and
  validation operations. WS-15 will not implement that shape until WS-01 amends
  the frozen template schema.

## Offline drift guard

WS-15 tests derive the unique action set from every shipped `construction_plan`,
require this handoff to name each action and owner, and verify the corresponding
registration sources plus Core transaction coordinator exist. Adding a new plan
action without a landed registration is therefore an offline test failure rather
than a runtime surprise.
