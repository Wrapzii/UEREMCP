# WS-05 handoff: expose Core job actions

**From:** WS-05  
**To:** WS-03 (Core), WS-04 (Transport), WS-01 (catalog/orchestration)  
**Status:** protocol adapter implemented; unowned wrappers remain

## Landed adapter

`FUeremcpJobActions` in `UeremcpProtocol` implements:

- `GetJobResult(RequestJson)` for `action: get_job_result`
- `CancelJob(RequestJson)` for `action: cancel_job`

Both parse the frozen request envelope, require only
`specification: {"job_id":"..."}`, use the process-wide
`FUeremcpJobRegistry::Get()`, return accepted response envelopes, and preserve
cumulative poll metrics. Cancel invokes the registry's cooperative callback and
returns honest results for cancelled, already-terminal, pending, rejected,
non-cancellable, and missing jobs.

Specification schema:
`schemas/domains/_shared/job_action.schema.json`.

## WS-03 exact wrapper

Add to `UUeremcpReferenceToolset` (or the settled Core public toolset):

```cpp
UFUNCTION(meta = (AICallable), Category = "UEREMCP")
static FString GetJobResult(const FString& RequestJson);

UFUNCTION(meta = (AICallable), Category = "UEREMCP")
static FString CancelJob(const FString& RequestJson);
```

Implement without additional parsing:

```cpp
FString UUeremcpReferenceToolset::GetJobResult(const FString& RequestJson)
{
    return FUeremcpJobActions::GetJobResult(RequestJson);
}

FString UUeremcpReferenceToolset::CancelJob(const FString& RequestJson)
{
    return FUeremcpJobActions::CancelJob(RequestJson);
}
```

`UeremcpCore` already privately depends on `UeremcpProtocol`; include
`UeremcpJobActions.h`. Add Core automation that creates a job in
`FUeremcpJobRegistry::Get()`, calls each UFUNCTION directly, and confirms the
current request ID, stable job ID/state, and cumulative round trips.

## WS-04 next unskip

After the WS-03 wrapper lands, remove the residual informational SKIPs in:

- `UEREMCP.Transport.JobRegistry.Poll`: invoke the Core `GetJobResult` wrapper
  against the shared registry instead of directly calling `GetJobResult`.
- `UEREMCP.Transport.JobRegistry.Cancel`: invoke the Core `CancelJob` wrapper
  and assert the cooperative callback runs once and returned state is
  `cancelled`.

The registry lifecycle assertions in WS-04 commit `e7d8172` remain valid.

`UEREMCP.Transport.Timeout.PartiallyCompleted` must retain its residual note
until a production dispatcher schedules work, waits `timeout_ms`, calls
`GetTimeoutResponse`, and returns without waiting for completion.

## WS-01 catalog

Register two capabilities using the shared specification schema:

- `get_job_result` — read/poll; process-local, non-destructive
- `cancel_job` — cooperative mutation of job execution state

No envelope or uplugin change is requested.

## Cancellation limitation

The public `cancel_job` action is functional. Epic MCP
`notifications/cancelled` still does not reach ToolsetRegistry-backed tools
because its adapter has no `CancelAsync` override
`[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.
Notification bridging remains WS-03/WS-04 work; do not claim it from this
adapter commit.
