# RB-04 Transport residual run — 2026-07-30

- **Worktree:** `$UEREMCP_ROOT-ws04-cancel-hardening`
- **Branch:** `ws-04-transport-cancel-hardening`
- **Scope:** ADR-0009 timeout and cancellation closeout

## Results

| Check | Result |
|---|---|
| `python Plugins/UEREMCP/Source/UeremcpTransport/scripts/test_transport_constraints.py` | PASS — `cancel_job` active; Epic notification limitation closed |
| `python tools/validate_schemas.py` | PASS — 25 schemas valid; all references and examples valid |
| `python tools/check_ownership.py --ws WS-04` | PASS — all changed paths owned by WS-04 |
| Isolated `RunUAT BuildPlugin` | PASS — 183/183 actions; Transport sources compiled and linked |
| `UEREMCP.Transport.*` editor automation | PASS — 8/8 on isolated packaged-plugin host |

Build command:

```powershell
& "$UE_ROOT\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="$UEREMCP_ROOT-ws04-cancel-hardening\Plugins\UEREMCP\UEREMCP.uplugin" `
  -Package="$UEREMCP_ROOT-ws04-cancel-hardening-build" `
  -TargetPlatforms=Win64 -Rocket
```

The UBT action log compiled and linked the complete plugin, including
`UeremcpJobConstraints.cpp`, `UeremcpJobScheduler.cpp`, and
`UeremcpTransportAutomationTests.cpp`
`[VERIFIED-RUNTIME: RunUAT actions 151-158; ExitCode=0]`.

## Residual disposition

- **Closed in code, compiled, runtime automation pending:** production
  `timeout_ms` scheduler. The active test now dispatches through
  `FUeremcpJobScheduler`, observes `partially_completed`, polls the stable job, and
  verifies retained terminal completion.
- **Definitively closed as unsupported:** MCP `notifications/cancelled` cannot reach
  AICallable jobs. Epic invokes
  `IModelContextProtocolTool::CancelAsync`
  `[VERIFIED: ModelContextProtocolServer.cpp:697-728]`, while the private
  ToolsetRegistry adapter has no override
  `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.
- **Supported:** direct `get_job_result` and `cancel_job` AICallable wrapper
  coverage. `JobRegistry.Cancel` now drives the production scheduler and verifies
  worker stop, retained progress, one rollback checkpoint, honest status, and
  terminal polling
  `[VERIFIED-RUNTIME: editor_UEREMCP_Transport_20260730_143347.log]`.

## Closeout

The RE junction was not touched. The packaged plugin was loaded by an isolated
content-only host project using `AdditionalPluginDirectories`
`[VERIFIED: ProjectDescriptor.cpp:148-165]`. All eight Transport tests completed
successfully with no test-body SKIP.

Protocol notification cancellation and user-visible UEREMCP job cancellation remain
separate capability statements. See
`docs/proposals/ws-04-cancellation-hardening-closeout.md`.
