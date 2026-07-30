# WS-05 / WS-15 → WS-03: execute_plan transaction callbacks — accepted

- **From:** WS-05, WS-15
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** accepted (Core hooks landed per `ws-15-plan-handler-registration.md`)

## Response

WS-03 registered complete `FUeremcpPlanTransactionCallbacks` with
`FUeremcpPlanExecutor::SetTransactionCallbacks` via `FUeremcpPlanTransactionCoordinator`:

| Callback | Implementation |
|---|---|
| `Begin` | Reject when sandbox already active; `FGlobalSandbox::Enter`; sample `GetActiveUndoCount()` |
| `Commit` | `FGlobalSandbox::Persist(all)` + `Leave` |
| `Rollback` | `FGlobalSandbox::Discard` + `Leave`; unwind undo delta with `UToolsetLibrary::UndoTransaction` |

Registration runs at Core `PostEngineInit`; `ClearTransactionCallbacks` on module shutdown
(rolls back any open session first).

API: `Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpPlanTransactionCoordinator.h`

Automation: `UeremcpCore.PlanTransaction.*`

## Remaining execute_plan gates (not WS-03)

| Item | Owner |
|---|---|
| `SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest)` | WS-15 (Templates module) |
| `RegisterAction` for `create_vfx_material` / `create_niagara_effect` | WS-08 / WS-07 |
| Template validation post-steps / modifier deltas | WS-01 / WS-05 |
