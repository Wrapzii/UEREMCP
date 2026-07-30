# RB-04: Transport options, progress, cancellation, long-running jobs

- **Owner:** WS-04
- **Status:** not_started
- **Blocks:** ADR-0009 (long-running job model), WS-05's job design
- **Priority:** high

## Framing

ADR-0002 removed the "which language for the external server" question by adopting
Epic's in-process server. That inherits Epic's transport characteristics, whatever they
are. Master prompt §18 asks for queued jobs, job IDs, progress, cancellation, timeouts,
heartbeat, crash recovery, and resumable operations — and the response envelope has a
`job` block reserving space for them.

Whether any of that is achievable depends on facts we do not have.

## Questions

### A. Transport

1. Does `ModelContextProtocol` support **stdio** in addition to HTTP? Settings expose
   only `ServerUrlPath`, `ServerPortNumber`, `bAutoStartServer`
   `[VERIFIED: ModelContextProtocolSettings.h]`, which suggests HTTP only. Confirm from
   `ModelContextProtocolServer.h` / `Session.h`.
2. Does it implement Streamable HTTP / SSE, or plain request-response? This determines
   whether progress can be *pushed* or must be *polled*.
3. What MCP protocol version does it implement, and what capabilities does it negotiate
   (`ModelContextProtocolCapabilities.h`)?
4. Does it support MCP **resources** (`IModelContextProtocolResourceProvider.h`)? If so,
   large graph payloads might be served as resources rather than tool results — a
   potentially significant answer for the payload-size problem in ADR-0004 and
   `docs/WHY.md`. **Follow this up; it could change the design.**
5. Does it support MCP **notifications** / server-initiated messages? Required for
   pushed progress.
6. Multiple concurrent clients — supported? Relevant because the owner intends to run a
   swarm of agents against one editor. **Directly relevant to ADR-0006's concurrency
   assumptions.**
7. What happens to an in-flight tool call if the client disconnects?

### B. Long-running work

8. How long can a tool call take before something times out — HTTP layer, MCP client, or
   engine? Measure, do not assume.
9. Is there existing job/async infrastructure, or only `TFuture` per call
   (`ModelContextProtocolToolAsyncAction.h`, `UToolCallAsyncResult`)?
10. Can progress be reported mid-call? If not, the `job` block in the response envelope
    must be implemented by us as an explicit poll model: return immediately with a
    `job_id`, and the agent calls `get_job_result`.
11. Can a running tool call be cancelled?
12. Does the editor stay responsive during a long tool call, or does main-thread work
    block the UI? A tool that freezes the editor for 90 seconds is a usability failure
    even when it succeeds.
13. What happens on editor crash mid-job? Any recovery, or is state lost?

### C. Practicalities

14. Does the server need `bAutoStartServer = true` for our workflow, and what are the
    implications of enabling it by default in project config?
15. How does an MCP client discover the server — is `$PROJ/.mcp.json` the whole story?
16. What logging does the server emit, and is it enough for the observability
    requirements in master prompt §19?
17. Is `Optional/UnrealWatchMCP` in REAgentTools (Slate dialog / lockup detection)
    solving a real problem we will also hit — a modal dialog blocking a tool call
    indefinitely? Almost certainly yes. Determine whether we need equivalent protection.

## Deliverables

- [ ] A transport capability table: stdio / SSE / notifications / resources / concurrency
- [ ] A measured maximum practical tool-call duration — `[VERIFIED-RUNTIME]`
- [ ] A recommended job model for ADR-0009, with a clear statement of what the engine
      gives us versus what we must build
- [ ] A verdict on question 4 (resources for large payloads) flagged to WS-01
- [ ] A verdict on question 6 (concurrent clients) flagged to WS-01 — ADR-0006 assumes
      multiple agents share a project
- [ ] A recommendation on modal-dialog/lockup protection, to WS-12
