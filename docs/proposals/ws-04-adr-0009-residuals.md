# WS-04 proposal: ADR-0009 cancellation-notification residual

- **Owner:** WS-04
- **Status:** open blocker
- **Affected decision:** ADR-0009 cooperative cancellation
- **Requested owners:** WS-01 (architecture), WS-03 (Core registration)

## Closed in WS-04

`FUeremcpJobScheduler` now enforces positive `timeout_ms` on background work. It
creates and starts a process-local job, returns the first of terminal completion or
`partially_completed`, and leaves timed-out work available to `get_job_result`.
Epic explicitly permits an MCP result callback to run from any thread and marshals
it to the game thread before consuming it
`[VERIFIED: IModelContextProtocolTool.h:30-32]`.

The active Transport automation test invokes this production scheduler for both
`timeout_ms == 0` and a blocked positive-timeout operation, then polls the same
stable job to terminal completion
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpTransport/Private/Tests/UeremcpTransportAutomationTests.cpp]`.

## Residual that WS-04 cannot honestly close

MCP `notifications/cancelled` locates the active MCP tool and invokes its
`CancelAsync(RequestId)` method
`[VERIFIED: ModelContextProtocolServer.cpp:697-728]`. The public tool interface
provides this virtual hook, but its default implementation is a no-op
`[VERIFIED: IModelContextProtocolTool.h:91-97]`.

UEREMCP goal tools are registered as `AICallable` ToolsetRegistry functions. Epic's
adapter for those functions overrides `RunAsync` but does not override
`CancelAsync`
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`. That adapter is in
the `ModelContextProtocolEditor/Private` tree, so WS-04 cannot subclass or replace
it through a supported public header
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h path and RB-04 §What is public to an out-of-tree plugin]`.

The UEREMCP `cancel_job` AICallable action does reach the cooperative registry
callback and is covered by active automation. That is an explicit semantic cancel
request; it is not a mapping from MCP request cancellation.

## Why a native-tool workaround is not applied

`IModelContextProtocolModule::AddTool` can register a custom public
`IModelContextProtocolTool`
`[VERIFIED: IModelContextProtocolModule.h:24-46]`. Such a tool could override
`CancelAsync`, but registering every UEREMCP operation a second time as a native MCP
tool would bypass the accepted `AICallable`/ToolsetRegistry authoring path and can
collide with Epic's tool-search adapter. This is an architecture change, not a
Transport-local wiring fix.

## Requested resolution

Choose one:

1. Patch or extend Epic's ToolsetRegistry MCP adapter so `CancelAsync` forwards a
   stable request/job correlation hook.
2. Accept a UEREMCP-owned public native MCP adapter for long-running semantic tools,
   and update ADR-0002/Core registration before implementation.
3. Keep MCP notification cancellation unsupported and advertise cancellation only
   through the explicit `cancel_job` action.

Until that choice lands, tests retain one honest `SKIP residual` marker and no tool
may claim that MCP `notifications/cancelled` stops AICallable domain work.
