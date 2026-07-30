# WS-04 JobRegistry test-unskip gate

**Owner requested:** WS-05 (registry) and WS-03 (agent-facing actions).
**Consumer:** WS-04 Transport automation.
**Status:** blocked — implementation exists only as uncommitted WS-05 working-tree
files as of 2026-07-30; no registry symbols are present on `ws-01-orch` at
`8c5a325`.

## Evidence and current gate

- `ws-01-orch` contains no `FUeremcpJobRegistry` definition and still marks
  `get_job_result` as planned in `docs/CAPABILITY_CATALOG.md`.
- The `ws-05-protocol` working tree contains untracked
  `Public/UeremcpJobRegistry.h`, `Private/UeremcpJobRegistry.cpp`, and
  `Private/Tests/UeremcpJobRegistryTests.cpp`; therefore these are not a landed
  dependency and WS-04 must not copy or consume them yet.
- The draft registry exposes process-local creation, polling, timeout-envelope,
  completion, and cooperative-cancel operations
  `[VERIFIED: ws-05-protocol working tree,
  Plugins/UEREMCP/Source/UeremcpProtocol/Public/UeremcpJobRegistry.h:63-120]`.
- The draft tests exercise poll accounting, terminal retention, cooperative
  cancellation, and timeout-envelope fields
  `[VERIFIED: ws-05-protocol working tree,
  Plugins/UEREMCP/Source/UeremcpProtocol/Private/Tests/UeremcpJobRegistryTests.cpp:11-132,242-267]`.
- No agent-facing `get_job_result` or cancel action is present in that draft. The
  WS-05 integration proposal delegates those registrations to Core/WS-03
  `[VERIFIED: ws-05-protocol working tree,
  docs/proposals/ws-05-job-registry-integration.md:31-63]`.

`JobRegistry.Poll` and `JobRegistry.Cancel` therefore remain explicit SKIPs.
`Timeout.PartiallyCompleted` is already an active response-shape test on WS-04 at
`e776a5f`, but the registry-dependent blocked-work, release, and terminal-poll
assertions remain gated. Treating untracked files from another worktree as a callable
dependency would violate ownership and would not survive a clean checkout.

## Required landed production surface

Land an exported Protocol header and implementation with equivalent behavior to:

```cpp
class UEREMCPPROTOCOL_API FUeremcpJobRegistry
{
public:
    bool CreateJob(..., FString& OutJobId, FString& OutError,
        TFunction<bool()>&& RequestCancel = TFunction<bool()>());
    bool StartJob(const FString& JobId, FString& OutError);
    bool CompleteJob(const FString& JobId,
        const FUeremcpResponse& TerminalResponse, FString& OutError);
    EUeremcpCancelResult CancelJob(const FString& JobId, FString& OutError);
    bool GetTimeoutResponse(const FString& JobId,
        FUeremcpResponse& OutResponse, FString& OutError);
    bool GetJobResult(const FString& JobId,
        FUeremcpResponse& OutResponse, FString& OutError);
};
```

Names may change before landing, but the callable behavior may not omit:

1. stable, non-empty UEREMCP job IDs scoped to the editor process;
2. queued/running/terminal state and legal transition enforcement;
3. retained terminal envelopes without re-executing work;
4. `metrics.mcp_round_trips == 1 + successful poll count`;
5. a timeout response that does not increment the poll count;
6. cancellation advertised only with an installed cooperative callback;
7. callback invocation outside the registry lock, including deterministic
   repeat-cancel behavior; and
8. structured not-found/error results without registry mutation.

WS-03 must separately register the public `get_job_result` and cancel actions before
UEREMCP can claim agent-facing poll/cancel support. Direct registry tests can unskip
once the Protocol symbols land; `UeremcpTransport.Build.cs` already depends on
`UeremcpProtocol`. Action-level integration remains a distinct gate.

## WS-04 assertions after landing

### `UEREMCP.Transport.JobRegistry.Poll`

- Create a unique job and assert a stable non-empty ID.
- Poll queued/running state and assert `partially_completed`,
  `poll_action == "get_job_result"`, and two MCP round trips.
- Complete with a terminal validated fixture, poll again, and assert the exact
  terminal envelope is retained with three round trips and no re-execution.
- Reject an empty/malformed/unknown ID and prove registry size/state did not change.

### `UEREMCP.Transport.JobRegistry.Cancel`

- Prove a job without a callback is not cancellable.
- Start a cancellable job whose callback flips a deterministic checkpoint.
- Cancel it, assert the callback ran, then poll `state == "cancelled"` and
  `cancellable == false`.
- Assert a repeated cancel reports the explicit terminal result and never invokes
  the callback twice.
- Assert unknown and already-terminal cancellation return non-success results.
- Attempt completion after cancellation and assert the terminal state cannot become
  completed.

### `UEREMCP.Transport.Timeout.PartiallyCompleted`

- For `timeout_ms == 0`, execute the deterministic fixture inline and assert a
  terminal envelope with no job timeout response.
- For `timeout_ms > 0`, keep the fixture blocked with an event/barrier, obtain
  `partially_completed` plus the running job handle, and assert one MCP round trip.
- Release and complete the same job, then poll the terminal envelope; assert the
  stable ID and initial-call-plus-polls metric.
- Do not use sleeps or wall-clock duration as proof of blocked state.

## Ownership-safe next step

WS-05 should commit and land the registry implementation with its Protocol tests.
WS-03 should land the action registrations. WS-04 already has the Protocol module
dependency; after the registry lands it will replace the two remaining SKIP bodies
and extend the active timeout test with lifecycle assertions, followed by the
existing `UEREMCP.Transport` editor filter.

The last recorded runtime evidence remains **5 PASS + 3 SKIP**. Current source has
**two explicit SKIPs plus one active timeout response-contract test**, but that is
static inspection, not a claim of a new editor run.
