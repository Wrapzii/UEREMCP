# WS-04 JobRegistry test-unskip gate

**Owner requested:** WS-05 (registry) and WS-03 (agent-facing actions).
**Consumer:** WS-04 Transport automation.
**Status:** registry gate landed at `de51038`; Core-facing JSON adapters landed at
`3106c1b`; AICallable wrappers landed at `2c35730`. Production timeout dispatch
remains blocked.

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
- `FUeremcpJobActions::GetJobResult` and `CancelJob` now map request envelopes to
  the process registry and return structured response envelopes
  `[VERIFIED: 3106c1b,
  Plugins/UEREMCP/Source/UeremcpProtocol/Private/UeremcpJobActions.cpp:88-159]`.
- `UUeremcpReferenceToolset::GetJobResult` and `CancelJob` expose the adapters as
  AICallable wrappers
  `[VERIFIED: 2c35730,
  Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpReferenceToolset.h:54-72]`.

Poll and Cancel now execute direct registry lifecycle assertions plus the AICallable
ReferenceToolset wrappers. They have no remaining SKIP. Timeout retains the
production dispatcher/SSE residual SKIP.

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

The AICallable wrappers, direct registry tests, and wrapper mapping tests are active.
`UeremcpTransport.Build.cs` depends on both `UeremcpCore` and `UeremcpProtocol`.
Live MCP tool discovery remains runtime evidence rather than an implementation gate.

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

The remaining implementation gate is the production timeout dispatcher and SSE
close behavior. WS-04 can replace that final residual SKIP with timeout/SSE
assertions once the dispatcher lands, then run the existing `UEREMCP.Transport`
editor filter without retargeting the RE junction.

The last recorded runtime evidence remains **5 PASS + 3 SKIP**. Current source has
fully active Poll and Cancel wrapper tests and one timeout/SSE residual SKIP, but
that is static inspection, not a claim of a new editor run.
