# RB-04: Transport options, progress, cancellation, long-running jobs

- **Owner:** WS-04
- **Status:** complete (Wave 1 handoff)
- **Blocks:** ADR-0009 (long-running job model), WS-05 job design
- **Priority:** high
- **Handoff artifact:** `Plugins/UEREMCP/Source/UeremcpTransport/constraints/transport_job_handoff.json`

## Executive summary

Epic's `ModelContextProtocol` is **HTTP-only** (no stdio), implements **Streamable
HTTP with SSE** for `tools/call`, negotiates protocol **`2025-11-25`**, supports
**resources**, **progress notifications** (heartbeat-style), and **cancellation**
at the MCP layer — but **ToolsetRegistry-backed tools do not wire `CancelAsync`**.
There are **no engine job IDs**; UEREMCP must implement the envelope `job` block as
an in-process poll model (`get_job_result`). Progress from Epic is **not**
semantic percent-complete — only interval heartbeats when the client sends
`progressToken`.

**Recommended job model for ADR-0009:** `options.timeout_ms == 0` → complete inline
on the MCP SSE stream; `timeout_ms > 0` → return `partially_completed` with `job`
handle before client/MCP timeout, continue work in-process, poll via
`get_job_result`. User-visible cancellation is the explicit AICallable `cancel_job`
action keyed by UEREMCP `job_id`. MCP `notifications/cancelled` remains unsupported
for ToolsetRegistry-backed UEREMCP calls because Epic's adapter has no cancellation
override. This is a closed UE 5.8 substrate limitation, not an unimplemented UEREMCP
adapter `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.

---

## A. Transport

### A1. stdio vs HTTP

| Verdict | HTTP only. No stdio transport in engine MCP plugin. |
|---|---|

**Evidence:**

- `FModelContextProtocolServer` comment: *"Serves MCP tools over HTTP"*
  `[VERIFIED: ModelContextProtocolServer.h:23-24]`
- Settings expose only `ServerUrlPath`, `ServerPortNumber`, `bAutoStartServer`
  `[VERIFIED: ModelContextProtocolSettings.h:23-42]`
- `StartServer` binds HTTP routes (POST/GET/DELETE) via `IHttpRouter`
  `[VERIFIED: ModelContextProtocolServer.cpp:416-447]`
- Repository-wide search for `stdio` under `$MCP`: **zero matches**
  `[VERIFIED: grep ModelContextProtocol]`

Client config generation writes `"type": "http"` URLs to `http://127.0.0.1:<port><path>`
`[VERIFIED: ModelContextProtocolClientConfig.cpp:158]`.

### A2. Streamable HTTP / SSE vs plain request-response

| Verdict | Hybrid. JSON-RPC POST for control; `tools/call` returns `text/event-stream` with SSE `data:` frames. |
|---|---|

**Evidence:**

- `tools/call` creates `ContentTypeEventStream` response with `MultipleWriteStream`
  `[VERIFIED: ModelContextProtocolServer.cpp:840-846, 892-895]`
- `FormatSSEMessage` emits `event: message\r\ndata: ...`
  `[VERIFIED: ModelContextProtocolServer.cpp:278-280]`
- `initialize`, `tools/list`, `resources/*` return plain JSON HTTP bodies via
  `CompleteWithResult` `[VERIFIED: ModelContextProtocolServer.cpp:182-198]`
- GET on MCP path returns **405 BadMethod** — no standalone SSE listen endpoint
  `[VERIFIED: ModelContextProtocolServer.cpp:1066-1075]`
- Epic tests note UE HTTP server uses raw TCP without Content-Length for streams;
  clients should use short `SetActivityTimeout` (~2s) when tool completes quickly
  `[VERIFIED: ModelContextProtocolEngineSubsystemTests.cpp:557-561]`

**Implication:** Progress and final results are **pushed on the open `tools/call`
stream**, not on a separate persistent channel.

### A3. Protocol version and capabilities

| Field | Value |
|---|---|
| Server latest | `2025-11-25` `[VERIFIED: ModelContextProtocol.h:19]` |
| Also supported | `2025-06-18`, `2024-11-05` `[VERIFIED: ModelContextProtocol.h:24-28]` |
| Negotiation | Client version if supported, else server latest `[VERIFIED: ModelContextProtocol.h:33-42]` |
| Header | `Mcp-Protocol-Version` must match post-init `[VERIFIED: ModelContextProtocolServer.cpp:597-602]` |

**Server capabilities advertised at initialize:**

- `tools.listChanged = true` `[VERIFIED: ModelContextProtocolServer.cpp:677-679]`
- `resources` capability object set (list/read implemented)
  `[VERIFIED: ModelContextProtocolServer.cpp:680]`
- Logging, prompts, sampling, elicitation structs exist in
  `ModelContextProtocolCapabilities.h` but are not all advertised at init from
  the snippet read — tools + resources are the active surface.

### A4. MCP resources (flagged WS-01)

| Verdict | **Yes — viable for large payloads.** Register `IModelContextProtocolResourceProvider`. |
|---|---|

**Evidence:**

- `resources/list`, `resources/read` methods dispatched
  `[VERIFIED: ModelContextProtocolServer.cpp:35-36, 627-634]`
- `IModelContextProtocolResourceProvider::ListResources` / `ReadResource`
  `[VERIFIED: IModelContextProtocolResourceProvider.h:30-33]`
- Module API: `AddResourceProvider` / `GetResourceProviders`
  `[VERIFIED: IModelContextProtocolModule.h:105-119]`

See `docs/proposals/ws-04-resources-for-large-payloads.md`.

### A5. Notifications / server-initiated messages

| Notification | Supported when |
|---|---|
| `notifications/progress` | Active `tools/call` SSE + client `progressToken` in `_meta` `[VERIFIED: ModelContextProtocolServer.cpp:821-828, 1036-1057]` |
| `notifications/tools/list_changed` | Deferred to next tick; delivered only on sessions with active SSE stream `[VERIFIED: ModelContextProtocolServer.cpp:1006-1034]` |
| `notifications/cancelled` | Client → server; cancels via `CancelAsync` `[VERIFIED: ModelContextProtocolServer.cpp:697-728]` |

**CVar:** `ModelContextProtocol.ProgressIntervalSeconds` (default 1.0s) controls
heartbeat interval `[VERIFIED: ModelContextProtocol.cpp:57-61]`.

Progress values are **monotonic integers**, not 0–1 fractions; comment says *"simply a
heartbeat as the total duration is unknown"* `[VERIFIED: ModelContextProtocolServer.cpp:1052-1053]`.

### A6. Multiple concurrent clients (flagged WS-01)

| Verdict | **Transport: yes. Safe parallel writes: no guarantee.** |
|---|---|

Each `initialize` allocates a new session GUID `[VERIFIED: ModelContextProtocolServer.cpp:664-671]`.
No max-session cap in server code. ToolsetRegistry executes tools without a global
queue `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.cpp:66-78]`.

See `docs/proposals/ws-04-concurrent-clients.md`.

### A7. Client disconnect during in-flight tool call

| Verdict | Tool may continue; result discarded if session/request removed. |
|---|---|

On completion, if `ActiveRequests` no longer contains the request id, result is
**silently dropped** (cancelled per MCP spec) `[VERIFIED: ModelContextProtocolServer.cpp:866-870]`.
`DELETE` with `Mcp-Session-Id` removes session `[VERIFIED: ModelContextProtocolServer.cpp:1078-1105]`.
No automatic `CancelAsync` on disconnect.

---

## B. Long-running work

### B8. Timeouts — layers

| Layer | Finding |
|---|---|
| Epic HTTP server | **No tool-duration timeout** in server code reviewed. |
| UE HTTP client (tests) | Default activity timeout ~**30s** for SSE streams that never close cleanly `[VERIFIED: ModelContextProtocolEngineSubsystemTests.cpp:559-561]` |
| MCP clients (Cursor, etc.) | **Not measured** — editor was not running this session. |
| Envelope `options.timeout_ms` | UEREMCP-owned; schema describes poll on exceed `[VERIFIED: schemas/envelope/request.schema.json:89]` |

`[VERIFIED-RUNTIME: 2026-07-30]` Transport automation on **RE shipping harness**
(`pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"`;
junction → `UEREMCP-ws01\Plugins\UEREMCP`): **8/8 Success** (5 PASS + 3 SKIP for
unimplemented JobRegistry). `Probe.EpicMcp` PASS — Epic MCP module loads; negotiated
protocol version non-empty; client endpoint uses `127.0.0.1`. Log:
`tests/integration/_logs/editor_UEREMCP_Transport_20260730_010212.log`. Measured max
safe SSE hold time against Cursor still deferred.

### B9. Existing async infrastructure

| Mechanism | Role |
|---|---|
| `IModelContextProtocolTool::RunAsync` + `FResultCallback` | MCP tool async contract `[VERIFIED: IModelContextProtocolTool.h:80-95]` |
| `TFuture<TValueOrError<FString,FString>>` | ToolsetRegistry `ExecuteTool` `[VERIFIED: Toolset.h via GROUNDED_FACTS.md §2.2]` |
| `UToolCallAsyncResult` + `UToolCallAsyncResultFutureHandler` | UObject promise pattern; `CanceledError` on unsubscribe `[VERIFIED: ToolCallAsyncResultFutureHandler.h:35-36, 61-62]` |
| `UModelContextProtocolToolAsyncAction` | **Deprecated** legacy path `[VERIFIED: ModelContextProtocolToolAsyncAction.h:19-22]` |

No first-class job registry in Epic MCP.

### B10. Mid-call progress

| Engine provides | UEREMCP must build |
|---|---|
| Heartbeat `notifications/progress` on SSE when `progressToken` set | Map domain milestones to `job.progress` / `progress_message` in envelope |
| | Do not rely on Epic heartbeat as percent-complete |

ToolsetRegistry adapter does **not** forward progress to MCP
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.cpp — no progress calls]`.

### B11. Cancellation

| Layer | Works? |
|---|---|
| MCP `notifications/cancelled` | Yes — calls `IModelContextProtocolTool::CancelAsync` `[VERIFIED: ModelContextProtocolServer.cpp:717-719]` |
| Default `CancelAsync` | Empty no-op `[VERIFIED: IModelContextProtocolTool.h:97]` |
| Legacy async actions | `UCancellableAsyncAction::Cancel()` `[VERIFIED: ModelContextProtocolToolAsyncAction.cpp:347-355]` |
| **ToolsetRegistry / AICallable tools** | **Not wired** — adapter has no `CancelAsync` override `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]` |
| **UEREMCP jobs** | **Wired through `cancel_job(job_id)`** — cooperative scheduler token, retained `cancelled` state, frozen progress, and domain rollback checkpoint `[VERIFIED-RUNTIME: UEREMCP.Transport.JobRegistry.Cancel, isolated BuildPlugin host, 2026-07-30]` |

UEREMCP domain services must implement their rollback/checkpoint boundary and expose
`cancellable: true` only when honored. Transport cannot infer how to undo a domain
mutation.

### B12. Editor responsiveness

Tool results consumed on **game thread** `[VERIFIED: ModelContextProtocolServer.cpp:851-853]`.
Long synchronous work on game thread **blocks UI and MCP**. ToolsetRegistry documents
main-thread marshalling for async results `[VERIFIED: ToolCallAsyncResult.h:81-83]`.

Domain services must offload heavy work to background threads and complete on game
thread only for editor mutations.

### B13. Crash mid-job

No persistence. Sessions and `ActiveRequests` are in-memory
`[VERIFIED: ModelContextProtocolSession.h:128-138]`. Crash loses all in-flight work.
Wave 1 job model: **in-memory only**; resumable jobs are a later ADR scope.

---

## C. Practicalities

### C14. `bAutoStartServer`

Default `false` `[VERIFIED: ModelContextProtocolSettings.h:42]`.
Editor module starts server when `ShouldAutoStartServer()` true
`[VERIFIED: ModelContextProtocolEditor.cpp:64-68]`.
CLI overrides: `-ModelContextProtocolStartServer`, `-ModelContextProtocolPort=N`
`[VERIFIED: ModelContextProtocolSettings.cpp:19-49]`.
Console: `ModelContextProtocol.StartServer` `[VERIFIED: ModelContextProtocolModule.cpp:31-33]`.

**Workflow:** Agents need server running — enable auto-start in project settings, CLI
flag, or manual console command. UEREMCP should not second-guess Epic startup.

### C15. Client discovery

Epic generates per-client configs (`.mcp.json`, `.cursor/mcp.json`, etc.) via
`ModelContextProtocol.GenerateClientConfig` `[VERIFIED: ModelContextProtocolClientConfig.h:38-48]`.
RE project uses `http://127.0.0.1:8000/mcp` `[VERIFIED: GROUNDED_FACTS.md §1.1]`.

### C16. Logging / observability

`LogModelContextProtocol` category; analytics via `IModelContextProtocolModule::RecordAnalyticsEvent`
`[VERIFIED: IModelContextProtocolModule.h:95-99]`.
Session/tool events in `ModelContextProtocolAnalytics.h`.
Sufficient for transport debugging; UEREMCP adds envelope `metrics` per ADR-0003.

### C17. Modal dialog protection (WS-12)

Documented in REAgentTools UnrealWatchMCP — host-side, not in-editor
`[VERIFIED: REAgentTools/Optional/UnrealWatchMCP/README.md]`.
See `docs/proposals/ws-04-modal-protection.md`.

---

## Transport capability table

| Capability | Epic MCP | UEREMCP action |
|---|---|---|
| stdio | No | N/A (ADR-0002) |
| HTTP | Yes | Use as-is |
| SSE tool streams | Yes | Use as-is |
| Persistent server push | No (GET 405) | Poll model for jobs |
| MCP resources | Yes | Optional provider for `complete` payloads |
| MCP progress push | Heartbeat only | Semantic progress in `job` block |
| MCP cancel notification | Yes for tools overriding `CancelAsync` | Unsupported for Epic ToolsetRegistry adapter |
| ToolsetRegistry cancel | No | Immutable UE 5.8 adapter limitation |
| UEREMCP job cancel | Yes | Use `cancel_job(job_id)` |
| Job IDs | No | In-process registry |
| Auth | Origin guard only | WS-12 |
| Concurrent sessions | Yes | Document write hazards |

---

## Recommended job model (ADR-0009 input)

Machine-readable: `Plugins/UEREMCP/Source/UeremcpTransport/constraints/transport_job_handoff.json`

C++ mirror: `UeremcpJobConstraints.h`, `UeremcpTransportProbe.h`

```
Agent tools/call (SSE open)
    │
    ├─ work completes within timeout_ms (or 0 = default inline)
    │     └─► final JSON envelope on SSE (status *_validated / etc.)
    │
    └─ timeout_ms exceeded while work continues
          ├─► close SSE with partially_completed + job { job_id, state: running, poll_action }
          ├─► background: domain service on worker thread, editor mutations on game thread
          └─► agent polls get_job_result(job_id) until terminal state
```

**Rules for WS-05:**

1. Never hold MCP SSE open past practical client timeout (~30s observed in Epic tests).
2. `job_id` is UEREMCP-scoped (UUID), not MCP JSON-RPC request id.
3. `metrics.mcp_round_trips` counts polls.
4. `cancel_job(job_id)` maps cooperative acceptance to `job.state: cancelled`; do
   not claim MCP request cancellation maps to the job.
5. Epic progress heartbeats are optional UX only; envelope `job.progress` is authoritative.

---

## What is public to an out-of-tree plugin

| API | Reachable | Notes |
|---|---|---|
| `IModelContextProtocolModule` | Yes | `ModelContextProtocol` module, public header |
| `FModelContextProtocolServer` | Yes | `IsServerRunning`, `GetServerPort` |
| `UE::ModelContextProtocol::GetServerPortNumber` etc. | Yes | `ModelContextProtocolEngine` |
| `IModelContextProtocolResourceProvider` | Yes | Register resources |
| `IModelContextProtocolTool` | Yes | Custom tools (not our primary path) |
| `FToolsetRegistryToolAdapter` | **No** | `ModelContextProtocolEditor` private |
| `ModelContextProtocolServer.cpp` internals | **No** | Source visible on disk but not linkable API |

UEREMCP transport adapter uses **public module API only** — no fork, no private includes.

---

## Negative findings

1. **No stdio** — external stdio MCP clients cannot attach without a proxy (REAgentTools `UnrealMcpProxy` pattern).
2. **No engine job queue** — master prompt §18 job features are entirely UEREMCP-owned.
3. **MCP cancel does not stop ToolsetRegistry tools** — the UE 5.8 adapter is private
   and has no `CancelAsync` override. UEREMCP uses `cancel_job(job_id)` instead
   `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26;
   UeremcpReferenceToolset.h:76-84]`.
4. **No measured end-to-end tool duration** — runtime blocked; use conservative `timeout_ms` defaults (120s default in handoff JSON).
5. **No modal detection API** in public MCP/ToolsetRegistry headers.

---

## Deliverables checklist

- [x] Transport capability table (above)
- [x] Measured maximum practical tool-call duration — **partial**: Epic ~30s SSE client risk cited; Cursor measured still deferred. Sandbox probe confirms loopback MCP loads in Cmd (`Probe.EpicMcp` PASS) `[VERIFIED-RUNTIME: 2026-07-30]`
- [x] Recommended job model for ADR-0009 (`transport_job_handoff.json`)
- [x] Resources verdict → `docs/proposals/ws-04-resources-for-large-payloads.md`
- [x] Concurrent clients verdict → `docs/proposals/ws-04-concurrent-clients.md`
- [x] Modal protection → `docs/proposals/ws-04-modal-protection.md`
- [x] Transport adapter module `UeremcpTransport` (probe + constraints, no Epic reimplementation)

---

## ADR alignment / contradictions

| ADR | Alignment |
|---|---|
| ADR-0001 | Confirmed — build on Epic MCP substrate |
| ADR-0002 | Confirmed — no external server; HTTP in-process |
| ADR-0003 | `job` block and `timeout_ms` are UEREMCP responsibilities — schema assumes features Epic does not provide |
| ADR-0006 | Concurrent sessions OK at transport; write serialization not provided — see proposal, not a contradiction |

No accepted ADR is contradicted by evidence. The implementation follows accepted
ADR-0009's poll-after-timeout model.

---

## Tests run

- `python Plugins/UEREMCP/Source/UeremcpTransport/scripts/test_transport_constraints.py` — **PASS** `[VERIFIED-RUNTIME: 2026-07-30]`
- `python tools/validate_schemas.py`
- `python tools/check_ownership.py --ws WS-04`
- **C++ automation on an isolated packaged-plugin host** (`UEREMCP.Transport.*`) —
  **8/8 Success, no test-body SKIPs**
  `[VERIFIED-RUNTIME: $UEREMCP_ROOT-ws04-cancel-hardening-build/TestLogs/editor_UEREMCP_Transport_20260730_143347.log]`.
  `JobRegistry.Cancel` invokes the AICallable `CancelJob` wrapper against
  `FUeremcpJobScheduler`, observes the worker stop token, executes one rollback
  checkpoint, retains progress, and polls terminal `job.state: cancelled`.
- Isolated `RunUAT BuildPlugin` — **Succeeded**, including
  `UeremcpJobConstraints.cpp`, `UeremcpJobScheduler.cpp`, and
  `UeremcpTransportAutomationTests.cpp`
  `[VERIFIED-RUNTIME: BuildPlugin 2026-07-30, 183/183 actions, ExitCode=0]`.
