# ADR-0009: Long-running job model

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-05 job field semantics, WS-04/WS-12 timeout + cancel wiring, Wave 2 long operations
- **Depends on:** ADR-0002, ADR-0003, RB-04 (`docs/research/RB-04-transport-and-jobs.md`),
  handoff `Plugins/UEREMCP/Source/UeremcpTransport/constraints/transport_job_handoff.json`

## Context

Master prompt §18 requires progress, cancellation, and resumable long work. Epic's
MCP server already exposes Streamable HTTP with SSE for `tools/call`, heartbeat
`notifications/progress`, and `notifications/cancelled`
`[VERIFIED: ModelContextProtocolServer.cpp:840-846, 1036-1057, 697-728]` (RB-04).

What Epic does **not** provide:

- stdio transport `[VERIFIED: grep $MCP — zero stdio matches]` (RB-04)
- engine-level job IDs or a job registry (RB-04 B9)
- ToolsetRegistry cancel wiring — adapter has no `CancelAsync` override
  `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`
- semantic percent-complete progress — heartbeats are monotonic integers when a
  `progressToken` is present `[VERIFIED: ModelContextProtocolServer.cpp:1052-1053]`
- a server-side tool-duration timeout; Epic tests cite ~30s HTTP client activity
  timeout for SSE streams `[VERIFIED: ModelContextProtocolEngineSubsystemTests.cpp:557-561]`
- durable jobs across editor crash — sessions/`ActiveRequests` are in-memory
  `[VERIFIED: ModelContextProtocolSession.h:128-138]`

Holding an SSE `tools/call` open until a multi-minute Niagara/Blueprint compile
finishes is therefore unsafe for clients. The envelope already has `options.timeout_ms`
and a `job` response block shape (ADR-0003); this ADR freezes how they behave.

## Decision

We will implement an **in-process poll-after-timeout job model** on top of Epic HTTP
MCP. We will not invent a second transport.

1. **`options.timeout_ms == 0`** — complete the operation inline on the open
   `tools/call` SSE stream and return a terminal envelope status.
2. **`options.timeout_ms > 0`** — if work is still running when the timeout fires,
   return `status: partially_completed` with a `job` handle and close the SSE
   stream. Continue work in-process. The agent polls `get_job_result` (or the
   equivalent action named in `job.poll_action`) until a terminal state.
3. **Never hold MCP SSE open past practical client limits.** Default
   `timeout_ms` for long operations is **120000**; implementers must treat ~30s of
   silent SSE as a hard client risk even when the envelope default is higher.
4. **`job_id` is UEREMCP-scoped** (UUID), not the MCP JSON-RPC request id.
   Scope is **per editor process**. Wave 1 jobs are **in-memory only**; crash
   recovery is out of scope.
5. **Semantic progress** lives in envelope `job.progress` / `progress_message`.
   Epic heartbeat notifications are optional UX only and must not be treated as
   percent-complete.
6. **Cancellation** is cooperative and owned by UEREMCP domain services. Tools may
   advertise `cancellable: true` only when cancel is honored. MCP
   `notifications/cancelled` does not stop ToolsetRegistry `ExecuteTool` futures
   until we wire our own cancel path.
7. **`metrics.mcp_round_trips` counts poll calls.** Long jobs must not pretend to
   be one round trip when the agent polled N times.
8. **Large `response_detail: complete` payloads** may additionally be exposed via
   MCP `resources/read` (`IModelContextProtocolResourceProvider`) with a
   `resource_link` in the summary response. That complements the envelope; it does
   not replace `result` (see open questions / proposal response).

Normative machine-readable constraints for implementers:
`Plugins/UEREMCP/Source/UeremcpTransport/constraints/transport_job_handoff.json`
(`handoff_version: ws04-wave1-1`).

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Hold SSE open until the job finishes | Conflicts with measured/cited ~30s client activity timeout; disconnect drops the result `[VERIFIED: ModelContextProtocolServer.cpp:866-870]`. |
| External job queue / second MCP server | Violates ADR-0002; duplicates Epic transport. |
| Rely on MCP progress + cancel alone | Progress is heartbeat-only; ToolsetRegistry cancel is unwired. Would report false cancellability. |
| Persistent/resumable jobs across editor restart | No engine persistence; adds storage and security scope before Wave 1 host model is proven. Deferred. |
| stdio sidecar for long jobs | No engine stdio; would be a new external server. Rejected under ADR-0002. |

## Consequences

**Enables:** multi-minute compiles and batch plans without lying about completion;
honest `partially_completed` + poll; comparable `mcp_round_trips` including polls;
ADR-0003 `job` fields become implementable.

**Costs:** agents must poll; in-flight work is lost on editor crash; cooperative
cancel must be built per domain; concurrent MCP sessions still do not serialize
editor writes (R-12 / WS-12).

**Locks in:** poll-based job handles as the long-running pattern for UEREMCP. Changing
to a push-only model later would require either a new transport or a persistent SSE
channel Epic does not expose (GET on the MCP path returns 405)
`[VERIFIED: ModelContextProtocolServer.cpp:1066-1075]`.

## Open questions

1. Exact measured max safe SSE hold time against Cursor/other clients —
   `[VERIFIED-RUNTIME]` deferred; editor was down during RB-04. Revisit defaults
   when measured.
2. Whether `complete` graph bodies are normative as MCP resources vs inline-only —
   accepted as **allowed**; URI scheme and ownership assigned after
   `docs/proposals/ws-04-resources-for-large-payloads.md` implementation lands.
3. Crash-resumable jobs — later ADR if production use demands it.
4. Optional single-mutator job queue for multi-agent writers — WS-12 / Wave 2
   (`docs/proposals/ws-04-concurrent-clients.md`).

## Verification

- Unit: timeout path returns `partially_completed` with `job.job_id` and
  `job.poll_action` without leaving an SSE wait.
- Integration: `get_job_result` reaches a terminal envelope status; polls increment
  `metrics.mcp_round_trips`.
- Cancel: when `cancellable: true`, a cancel transitions `job.state` to `cancelled`
  and stops domain work; when not wired, tool must not claim cancellable.
- Conform to `transport_job_handoff.json` `ws05_constraints`.
