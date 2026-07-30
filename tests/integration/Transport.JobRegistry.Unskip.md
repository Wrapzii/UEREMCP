# Transport JobRegistry unskip handoff

**Owner:** WS-11. **Status:** prepared; JobRegistry tests remain skipped.

The shipping Transport filter previously completed with five passes and three explicit
skips `[VERIFIED-RUNTIME: editor_UEREMCP_Transport_20260730_010212.log; recorded in
docs/research/RB-04-transport-and-jobs.md]`. The raw log is not present in the WS-11
worktree, so this document is not a replacement redaction and does not claim a new run.

Current skipped automation paths:

- `UEREMCP.Transport.JobRegistry.Poll`
- `UEREMCP.Transport.JobRegistry.Cancel`
- `UEREMCP.Transport.Timeout.PartiallyCompleted`

Their current bodies only emit `SKIP:` information and return success
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpTransport/Private/Tests/UeremcpTransportAutomationTests.cpp:225-265]`.
Therefore an eight-test `Success` summary currently means **5 PASS + 3 SKIP**, not 8 PASS.

## Landing trigger

Do not guess the WS-05 API. Start implementation only after the owning workstream lands
all callable production symbols needed to:

1. submit or register deterministic in-process work,
2. poll a job by stable `job_id`,
3. request cooperative cancellation,
4. enforce `timeout_ms` and return an envelope while work continues, and
5. observe terminal state and `metrics.mcp_round_trips`.

Record the landed commit and exact headers/functions before replacing a skip. Every API
claim in the implementation must cite the landed source with `[VERIFIED: path:line]`.
If the symbols land outside WS-11 ownership, WS-11 edits tests only.

## Deterministic fixture requirements

- Use a controllable test job with barriers/events; do not use fixed sleeps as proof of
  running or cancellation state.
- Do not touch project content. If a domain fixture is unavoidable, restrict it to
  `/Game/__UeremcpTests/` and guarantee cleanup.
- Keep work in-process. Restart durability is outside ADR-0009 Wave 1 scope.
- Use unique job and request IDs per test so parallel or stale runs cannot alias.
- A negative result is valid evidence; do not preserve green by converting a failed
  assertion back into `SKIP:`.

## Poll acceptance

Replace `JobRegistry.Poll` skip only when the fixture verifies:

1. initial submission returns a non-empty stable `job_id`;
2. polling before release reports a non-terminal queued/running state;
3. releasing the fixture reaches one terminal state;
4. terminal result can be retrieved without re-executing the operation;
5. unknown and malformed IDs return structured failures without mutation; and
6. reported `metrics.mcp_round_trips` includes every poll.

The test result may be called PASS only when all six assertions execute. Compilation or
registry construction alone is not a poll pass.

## Cancel acceptance

Replace `JobRegistry.Cancel` skip only when the fixture verifies:

1. non-cancellable work does not advertise `cancellable: true`;
2. cancellation of cancellable running work is accepted;
3. the job reaches `cancelled` and does not later transition to `completed`;
4. domain work observes a cooperative cancellation checkpoint and stops;
5. repeat cancellation has an explicit idempotent or conflict result; and
6. cancellation of an unknown/terminal job returns a structured non-success result.

MCP `notifications/cancelled` alone is insufficient because the ToolsetRegistry adapter
does not provide the required UEREMCP domain cancellation behavior
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.

## Timeout acceptance

Replace `Timeout.PartiallyCompleted` skip only when the fixture verifies both branches:

- `timeout_ms == 0`: execution stays inline and returns a terminal envelope.
- `timeout_ms > 0`: a blocked fixture returns `partially_completed` with a job handle,
  then completes after release and is retrieved through the poll action.

Also assert that the early response does not report a terminal success, the job handle
is stable across polls, and final metrics count the initial call plus polls. Do not use
wall-clock timing alone; coordinate the fixture so work is known to be blocked when the
timeout response is produced.

## Runtime handoff

After all three skip bodies are replaced and the plugin compiles on the existing orch
junction:

```powershell
pwsh tests/run_editor_tests.ps1 `
  -KeepUeremcp -NoProbe `
  -Filter "UEREMCP.Transport"
```

Do not retarget the RE junction. If the editor is owned by another lane, defer the run.
Commit only a redacted note containing branch/commit, existing junction target, command,
exit code, all eight per-test results, skip count, and limitations.

The completion condition is **8 PASS, 0 SKIP, 0 FAIL**. Until that evidence exists,
report the historical result exactly as **5 PASS + 3 SKIP**.
