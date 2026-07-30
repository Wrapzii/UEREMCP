# WS-04 production timeout and cancellation-notification residuals

**Owner requested:** WS-03 for Core/domain dispatch; WS-01 for any architecture or
Epic-integration decision.
**Consumer:** WS-04 Transport verification.
**Status:** proposed from verified UE 5.8 and landed UEREMCP surfaces.

## What is already complete

The following path is implemented and covered by active Transport tests:

1. `UUeremcpReferenceToolset::GetJobResult` and `CancelJob` are AICallable wrappers
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpReferenceToolset.h:54-72]`.
2. The wrappers delegate unchanged request JSON to `FUeremcpJobActions`
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpReferenceToolset.cpp:55-63]`.
3. The actions map request envelopes to the process-wide registry and return
   structured response envelopes
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Private/UeremcpJobActions.cpp:88-159]`.
4. The registry retains terminal results, counts polls, and invokes cooperative
   cancellation callbacks
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Private/UeremcpJobRegistry.cpp:355-482]`.

The remaining work is not another poll/cancel action. It is (a) dispatching long work
so the initiating action can return before work completes and (b) deciding whether
MCP request cancellation can be supported for ToolsetRegistry-backed calls.

## Verified Epic transport behavior

Epic opens an SSE response for `tools/call`, stores the selected tool and stream
callback in the session's active-request map, and invokes the tool asynchronously
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Private/ModelContextProtocolServer.cpp:778-851]`.

When the tool callback returns a result, Epic serializes the result to the SSE stream
and removes the active request
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Private/ModelContextProtocolServer.cpp:851-894]`.
Therefore a UEREMCP action can close its initiating tool stream with
`partially_completed` by completing its ToolsetRegistry future with that envelope.
No private server call or second transport is required.

The ToolsetRegistry bridge completes its MCP callback only when
`FToolsetRegistry::ExecuteTool(...).Then(...)` resolves
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.cpp:36-78]`.
The production timeout seam must therefore be inside the UEREMCP action/domain
execution path, before that future resolves.

For cancellation, Epic parses `notifications/cancelled.params.requestId`, finds the
active MCP request, invokes `Context->Tool->CancelAsync(requestId)`, removes the
active request, and accepts the notification
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Private/ModelContextProtocolServer.cpp:697-728]`.

`IModelContextProtocolTool::CancelAsync` is a default no-op
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Public/IModelContextProtocolTool.h:91-97]`.
Epic's `FToolsetRegistryToolAdapter` overrides `RunAsync` but does not override
`CancelAsync`
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.
That adapter is in a private Editor-module header, not a public extension point
`[VERIFIED: docs/research/RB-04-transport-and-jobs.md:292-304]`.

## Production timeout handoff

WS-03/domain owners need one shared dispatch contract around actual long work:

1. Parse and normalize `options.timeout_ms` using the landed Protocol rules.
2. For `timeout_ms == 0`, execute inline and return the terminal envelope.
3. For `timeout_ms > 0`, create and start a registry job before dispatching work.
4. Install a cooperative cancellation callback only when the domain has a real
   checkpoint; otherwise keep `cancellable: false`.
5. Start the domain operation exactly once. UObject mutation must remain on a safe
   editor/game-thread path; this proposal does not infer a worker-thread-safe Unreal
   mutation API.
6. If work completes before the deadline, store and return its terminal envelope.
7. If the deadline expires first, return
   `FUeremcpJobRegistry::GetTimeoutResponse` immediately while the same operation
   continues in-process.
8. On eventual completion/failure/cancellation, store exactly one terminal result
   for `get_job_result`; never re-execute the original request during a poll.

The dispatcher must own the lifetime of request data, cancellation state, and any
security/mutator lease after the initiating UFUNCTION returns. The current
`FUeremcpMutatingDispatch` is stack-scoped RAII
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpMutatingDispatch.h:12-69]`;
moving a mutating operation past the initiating return therefore needs an explicit
owner-approved lifetime design. WS-04 must not silently hold or reconstruct that
lease.

## Cancellation-notification decision

The supported cancellation path today is the explicit AICallable `cancel_job`
operation keyed by UEREMCP `job_id`. It is verifiable and must remain advertised.

MCP `notifications/cancelled` is keyed by the MCP JSON-RPC request id, not the
UEREMCP job id. No public mapping from the ToolsetRegistry adapter's request id to a
UEREMCP registry job is exposed, and the adapter's cancellation method is the
interface no-op. Consequently:

- Do not claim that `notifications/cancelled` stops ToolsetRegistry-backed UEREMCP
  work.
- Do not include Epic private adapter headers from UEREMCP.
- Do not register duplicate native MCP tools with the same names merely to gain a
  `CancelAsync` override; that would bypass the accepted AICallable substrate.
- Keep the explicit `cancel_job` action as the production cancellation mechanism.

If notification-driven cancellation is mandatory, WS-01/WS-03 must choose an
owner-approved seam backed by new verified evidence, such as a public Epic adapter
cancellation hook. Until such a seam exists, the notification residual remains an
honest limitation rather than an implementation task WS-04 can complete.

## Acceptance evidence required to remove SKIPs

Remove `Timeout.PartiallyCompleted`'s production residual only after one real
AICallable long operation proves both timeout branches, stable job identity, no
re-execution, terminal polling, poll metrics, and SSE completion without retargeting
the RE junction.

Remove the `notifications/cancelled` residual only after a real in-flight
ToolsetRegistry-backed request proves that Epic's notification reaches a UEREMCP
cooperative checkpoint and the job remains terminally cancelled. An Accepted response
to the notification alone is not proof because Epic returns it even when the adapter
does no cancellation work.
