# Long-running job model (protocol view)

**Owner:** WS-05  
**Status:** Normative for protocol helpers — frozen by ADR-0009 (WS-01)  
**Do not edit:** `docs/adr/ADR-0009-long-running-jobs.md` (WS-01 owned)  
**Transport handoff:** `Plugins/UEREMCP/Source/UeremcpTransport/constraints/transport_job_handoff.json`
(`handoff_version: ws04-wave1-1`) — snapshot of `ws05_constraints` below.

## Dispatch rules

| `options.timeout_ms` | Behaviour |
|---|---|
| `0` (or omitted) | Complete **inline** on the open MCP `tools/call` SSE stream. Return a terminal envelope status. |
| `> 0` | If still running when the timeout fires: return `status: partially_completed` with a `job` handle, **close the SSE stream**, continue work in-process. Agent polls `job.poll_action` (default `get_job_result`). |

Never hold MCP SSE open past practical client limits. Epic tests cite ~30s HTTP
client activity timeout for SSE streams — treat ~30s of silent SSE as a hard
client risk even when the envelope default for long ops is higher.

## Defaults (from transport handoff)

| Constant | Value |
|---|---|
| `default_timeout_ms` (long ops) | `120000` |
| `min_timeout_ms` | `1000` |
| `max_timeout_ms` | `600000` |
| `poll_action` | `get_job_result` |

Exposed in C++ as `FUeremcpJobDefaults` / Python `ueremcp_protocol.job`.

## `job` response block

Shape is frozen in `schemas/envelope/response.schema.json` (WS-01). Semantics:

| Field | Rule |
|---|---|
| `job_id` | UEREMCP UUID, **not** the MCP JSON-RPC request id. Scope: **per editor process**. Wave 1: **in-memory only**; crash recovery out of scope. |
| `state` | `queued` \| `running` \| `completed` \| `failed` \| `cancelled` |
| `progress` | Semantic progress in `[0, 1]`. **Not** Epic `notifications/progress` heartbeat counts. |
| `progress_message` | Human/agent-readable phase text. |
| `cancellable` | `true` **only** when UEREMCP cooperative cancel is wired and honored. Default `false` until then. Do not claim cancellable early. |
| `poll_action` | Action the agent calls with the job handle. Default `get_job_result`. |

## Metrics

`metrics.mcp_round_trips` **counts poll calls**. A long job that required N polls
must report those N round trips on the terminal response (or each poll response
increments; the sum across the goal is what `docs/WHY.md` measures). Never pretend
a polled job was a single round trip.

## Registry and helpers in this module

| API | Role |
|---|---|
| `FUeremcpJob` | Typed `job` block |
| `FUeremcpJobDefaults` | Timeout / poll defaults |
| `FUeremcpJobRegistry` | Thread-safe process-local lifecycle, poll, cooperative cancel, progress, capacity, and expiry |
| `FUeremcpJobSnapshot` | Non-polling state inspection for Core/tests |
| `FUeremcpPlanActions::ExecutePlan` | Agent-facing string adapter (Core AICallable target) |
| `FUeremcpJobRegistry::GetTimeoutResponse` | Initiating-call response without counting a poll |
| `FUeremcpEnvelope::MakeJobTimeoutResponse` | `partially_completed` + running job handle |
| `FUeremcpJobUtil::ShouldDispatchInline` | `timeout_ms == 0` → inline |
| `FUeremcpIdempotencyStore` | Unrelated session store; jobs are a separate registry (WS-03/Core wiring) |

The registry owns lifecycle state, not domain execution. Domain services dispatch
work, provide an honoring cancellation callback before advertising `cancellable:
true`, update semantic progress, and submit the verified terminal envelope. Core
registers `get_job_result` / cancellation actions and delegates to the registry.

State transitions are bounded to:

```text
queued -> running -> completed | failed | cancelled
queued ---------> failed | cancelled
```

Terminal states cannot reopen. Progress is `[0,1]` and monotonic. The default
registry retains at most 1024 entries, keeps terminal results for five minutes,
and converts active jobs older than 24 hours to an honest `failed` / `error`
result before later cleanup.

## Explicitly out of scope here

- Finalising batch `$ref` grammar (still blocked on WS-02)
- Editing ADR-0009 or envelope schemas (WS-01)
- Registering public `AICallable` Core actions (WS-03)
- Mapping MCP `CancelAsync` to Core cancellation (WS-04 / WS-03)
