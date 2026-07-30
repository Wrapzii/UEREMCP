# WS-05 handoff: ADR-0009 registry integration

**From:** WS-05  
**To:** WS-03 (Core), WS-04 (Transport), WS-01 (orchestration)  
**Status:** implementation handoff; no unowned path edited

## Landed WS-05 API

Module: `UeremcpProtocol`

- `FUeremcpJobRegistry::Get()` — process-local shared registry.
- `CreateJob(...)` — creates `queued`; only advertises cancellation when both
  `bCancellable` and an honoring callback are supplied.
- `StartJob(...)` — `queued -> running`.
- `UpdateProgress(...)` — semantic, monotonic `[0,1]`.
- `CompleteJob(...)` — accepts an envelope-valid non-`partially_completed`
  terminal response and transitions `running -> completed`.
- `FailJob(...)` — `queued|running -> failed` with `status: error`.
- `CancelJob(...)` — invokes the domain callback outside the registry lock and
  transitions to `cancelled` only when the callback returns true.
- `GetTimeoutResponse(...)` — initiating-call timeout envelope without
  incrementing the poll count (`mcp_round_trips == 1`).
- `GetJobResult(...)` — returns an accepted response envelope and increments
  cumulative `metrics.mcp_round_trips` (`1 + poll_count`).
- `CleanupExpired[At](...)` — removes retained terminal results and converts
  over-age active work to `failed`.
- `FUeremcpJobRegistryConfig` — defaults: 1024 retained entries, five-minute
  terminal retention, 24-hour active ceiling.

State machine:

```text
queued -> running -> completed | failed | cancelled
queued ---------> failed | cancelled
```

Terminal states cannot reopen.

## WS-03 exact Core work

1. Add two static `AICallable` actions to `UUeremcpReferenceToolset` (or the
   settled public Core toolset):
   - `GetJobResult(const FString& RequestJson)`
   - `CancelJob(const FString& RequestJson)`
2. Parse the normal request envelope. Require
   `specification.job_id` as a non-empty string.
3. `GetJobResult` delegates to
   `FUeremcpJobRegistry::Get().GetJobResult(JobId, Response, Error)` and
   serializes `Response` with `FUeremcpEnvelope::SerializeResponse`.
4. Missing/expired IDs return `status: rejected`, not a fabricated failed job.
5. `CancelJob` maps:
   - `Cancelled` -> poll the now-terminal registry result and return it.
   - `NotFound` -> `rejected`.
   - `NotCancellable` -> `rejected` with explicit cooperative-cancel limitation.
   - `AlreadyTerminal` -> return the existing terminal result.
   - `CancellationPending` -> `partially_completed`; do not invoke the callback twice.
   - `RejectedByWorker` -> `partially_completed`; the job remains active.
6. Domain dispatch order for `timeout_ms > 0`:
   create queued job -> dispatch work -> `StartJob` -> wait only for the
   normalized positive timeout -> if unfinished, return the registry's current
   `partially_completed` response through `GetTimeoutResponse`. For
   `timeout_ms == 0`, preserve inline completion per ADR-0009.
7. Call `CompleteJob` only after compile/save/re-read validation. A queued or
   running job is not validated completion.

Core already privately depends on `UeremcpProtocol`; no shared uplugin/schema
change is required.

## WS-04 exact unskip work

Add `"UeremcpProtocol"` to
`UeremcpTransport.Build.cs` private dependencies, then replace the three
placeholder bodies in
`Private/Tests/UeremcpTransportAutomationTests.cpp`:

### `UEREMCP.Transport.JobRegistry.Poll`

- Create a local `FUeremcpJobRegistry`.
- `CreateJob`, `StartJob`, first `GetJobResult`.
- Assert `status == "partially_completed"`, `job.state == "running"`,
  `job.poll_action == "get_job_result"`, and `mcp_round_trips == 2`.
- Complete with a valid `created_and_validated` response.
- Poll again; assert `job.state == "completed"` and
  `mcp_round_trips == 3`.

### `UEREMCP.Transport.JobRegistry.Cancel`

- Create/start with a callback that sets a test boolean and returns true.
- Assert `CancelJob == EUeremcpCancelResult::Cancelled`, callback boolean true.
- Poll and assert `job.state == "cancelled"`,
  `status == "partially_completed"`, and `cancellable == false`.
- Add a negative job without a callback; assert
  `EUeremcpCancelResult::NotCancellable`.

### `UEREMCP.Transport.Timeout.PartiallyCompleted`

- Assert `FUeremcpJobUtil::ShouldDispatchInline(0)`.
- For a created/started job representing positive-timeout work, call
  `GetTimeoutResponse` while incomplete and assert `partially_completed` +
  non-empty `job_id` + `state == "running"` + poll action +
  `mcp_round_trips == 1`.
- Complete and poll to terminal; this verifies the public envelope transition,
  but an end-to-end Core dispatch timer remains WS-03 coverage.

Remove `SkipMissingApi` only after all three bodies execute these assertions.

## WS-01 orchestration

Cherry-pick the WS-05 commit before WS-03/WS-04 integration commits. No envelope
or domain schema change is requested. The existing frozen `job` block and status
enum are used unchanged.

## Limitation

MCP `notifications/cancelled` still does not reach ToolsetRegistry-backed tools
through Epic's adapter
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`. The public
`cancel_job` action can be fully functional now; notification-to-action bridging
remains WS-03/WS-04 integration.
