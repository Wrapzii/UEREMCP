# WS-05 proposal: production timeout dispatcher (ADR-0009)

**From:** WS-05  
**To:** WS-03 (Core), WS-04 (Transport)  
**Status:** proposed — WS-05 does **not** own AICallable wrappers, MCP SSE
lifecycle, or editor scheduling

## Why this is not WS-05

`FUeremcpJobRegistry` and `FUeremcpJobActions` already implement:

- create / start / progress / complete / fail / cancel
- `GetTimeoutResponse` (initiating call, `mcp_round_trips == 1`)
- `GetJobResult` / `CancelJob` envelope adapters

What remains is **production scheduling**: wait `options.timeout_ms`, decide
whether work finished, return `partially_completed` without holding MCP SSE open,
and continue domain work on the correct thread. That requires ToolsetRegistry /
Core tool wiring and Transport awareness of SSE close — paths WS-05 does not own.

## Required Core dispatcher sketch

For any long goal-level tool with `timeout_ms > 0`:

1. `CreateJob(RequestId, bCancellable, ProgressMessage, JobId, Error, CancelCb)`
2. `StartJob(JobId, Error)`
3. Dispatch domain work (background for heavy work; editor mutations on game thread
   per RB-04 / GROUNDED_FACTS)
4. Wait only until the normalized positive timeout
5. If unfinished: `GetTimeoutResponse(JobId, Response, Error)` → serialize and
   return (closes SSE). Do **not** call `GetJobResult` on the initiating path.
6. If finished within timeout: `CompleteJob` after verification, return terminal
   envelope inline
7. Agent later polls via Core `GetJobResult` UFUNCTION wrapping
   `FUeremcpJobActions::GetJobResult`

For `timeout_ms == 0`: never create a poll handle; complete inline
(`FUeremcpJobUtil::ShouldDispatchInline`).

## Required Transport residual unskip

After Core wrappers + dispatcher land, `UEREMCP.Transport.Timeout.PartiallyCompleted`
can drop its residual SKIP and assert the production path (or a Core-owned
dispatcher unit) rather than only the registry fixture.

Cancel residual (MCP `notifications/cancelled` → Core cancel) remains Transport /
Core: Epic adapter has no `CancelAsync` override
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.

## Explicit non-goals for WS-05

- No `AICallable` on `UeremcpProtocol` (layering: Protocol stays ToolsetRegistry-free)
- No RE junction retarget
- No second job ID namespace — always `FUeremcpJobRegistry::Get()`
