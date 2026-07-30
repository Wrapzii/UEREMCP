# WS-04 JobRegistry test-unskip gate

**Owner requested:** WS-05 (registry) and WS-03 (agent-facing actions).
**Consumer:** WS-04 Transport automation.
**Status:** registry gate landed at `de51038` and consumed by WS-04; Core
agent-facing actions and production timeout dispatch remain blocked.

## Evidence and current gate

- WS-05 committed the registry as `de51038`; WS-04 consumed that commit after
  syncing the latest orchestration branch.
- The landed registry exposes process-local creation, polling, timeout-envelope,
  completion, and cooperative-cancel operations
  `[VERIFIED: de51038,
  Plugins/UEREMCP/Source/UeremcpProtocol/Public/UeremcpJobRegistry.h:63-120]`.
- The landed Protocol tests exercise poll accounting, terminal retention, cooperative
  cancellation, and timeout-envelope fields
  `[VERIFIED: de51038,
  Plugins/UEREMCP/Source/UeremcpProtocol/Private/Tests/UeremcpJobRegistryTests.cpp:11-132,242-267]`.
- No agent-facing `get_job_result` or cancel action is present in Core. The
  WS-05 integration proposal delegates those registrations to Core/WS-03
  `[VERIFIED: de51038,
  docs/proposals/ws-05-job-registry-integration.md:31-63]`.

The three Transport paths now execute registry lifecycle assertions directly.
Each records a precise residual SKIP for the missing Core action or production
timeout dispatcher rather than claiming end-to-end MCP coverage.

## Landed production surface

The exported Protocol header landed with this surface:

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

The callable behavior includes:

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
UEREMCP can claim agent-facing poll/cancel support. The direct registry tests are now
active, and `UeremcpTransport.Build.cs` already depends on `UeremcpProtocol`.
Action-level integration remains a distinct gate.

## Active WS-04 assertions

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

WS-03 should land the public `get_job_result` and cancel action registrations plus
the production timeout dispatcher. WS-04 can then replace the three residual
integration SKIP notes with action/SSE assertions and run the existing
`UEREMCP.Transport` editor filter without retargeting the RE junction.

The last recorded runtime evidence remains **5 PASS + 3 SKIP**. Current source has
three active registry lifecycle tests with three narrower residual integration SKIP
notes, but that is static inspection, not a claim of a new editor run.
