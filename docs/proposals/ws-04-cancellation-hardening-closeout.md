# WS-04 cancellation hardening closeout

- **Owner:** WS-04
- **Status:** ready for owner adoption
- **Requested owners:** WS-01 (`docs/CAPABILITY_CATALOG.md`), WS-12
  (`docs/SECURITY.md`), WS-13 (`docs/guide/**`)
- **Date:** 2026-07-30

## Decision

Close MCP protocol-level cancellation for ToolsetRegistry-backed UEREMCP tools as an
immutable UE 5.8 adapter limitation. Keep the explicit AICallable
`cancel_job(job_id)` action as the supported user-visible job cancellation path.

These are different capabilities:

- `notifications/cancelled` carries an MCP JSON-RPC request id. Epic finds the active
  MCP tool, invokes `CancelAsync`, removes the request, and returns HTTP 202
  `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Private/ModelContextProtocolServer.cpp:697-728]`.
- Custom `IModelContextProtocolTool` implementations can override `CancelAsync`
  `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Public/IModelContextProtocolTool.h:91-97]`.
- Epic's ToolsetRegistry adapter overrides only `RunAsync`; it has no
  `CancelAsync` override
  `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.
- Tool-search mode routes AICallable work through the private `FCallTool`, which also
  has no `CancelAsync` override
  `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolSearch.h:61-80]`.
- `cancel_job` carries a UEREMCP `job_id`, invokes the process-local cooperative
  callback, and returns the retained job envelope
  `[VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Private/UeremcpJobActions.cpp:105-159]`.

## Why the public native-tool hook is not used

`IModelContextProtocolModule::AddTool` is public, but tool names are globally unique;
a duplicate `call_tool` or duplicate goal tool is rejected
`[VERIFIED: IModelContextProtocolModule.h:24-46;
ModelContextProtocolModule.cpp:99-136]`.

Replacing Epic's `call_tool` at runtime is not a stable extension seam. Refresh clears
the complete tool collection and broadcasts provider callbacks, while Epic's Editor
module independently re-registers its private adapters on refresh and on every
ToolsetRegistry registration
`[VERIFIED: ModelContextProtocolModule.cpp:144-148;
ModelContextProtocolEditor.cpp:23-61]`.

A UEREMCP decorator would therefore depend on delegate registration order, would
intercept every engine toolset call, and would bypass ADR-0002's frozen AICallable
authoring path. No Engine patch, private include, duplicate native tool, or external
MCP server is introduced.

## Implemented and verified path

The transport handoff now publishes both facts independently:

- `toolset_registry_cancel_wired: false`
- `ueremcp_cancel_job_action: true`

`UEREMCP.Transport.JobRegistry.Cancel` now drives the production
`FUeremcpJobScheduler` through the AICallable `CancelJob` wrapper. The editor test
verifies the worker observes the cooperative token, executes exactly one domain
rollback checkpoint, stops without validated completion, retains progress at the
checkpoint, and remains pollable as terminal `job.state: cancelled`
`[VERIFIED-RUNTIME: isolated packaged-plugin host,
editor_UEREMCP_Transport_20260730_143347.log, 8/8 Success]`.

The rollback assertion is a transport/domain-boundary checkpoint, not a claim that
Transport can undo arbitrary assets. Asset rollback remains the domain's
FileSandbox/transaction responsibility under ADR-0005.

## Requested documentation updates

WS-01 should change the capability catalog entries to:

- `get_job_result`: available for process-local jobs; timeout integration remains
  domain-adoption dependent.
- `cancel_job`: available for jobs that advertise `cancellable: true`; cooperative
  scheduler path editor-verified.
- Add a separate limitation: MCP `notifications/cancelled` does not cancel
  ToolsetRegistry/AICallable work on UE 5.8.

WS-12 should add to `docs/SECURITY.md`:

- Cancellation is cooperative, not a kill primitive.
- Operators and agents must use `cancel_job(job_id)`.
- A `cancelled` job must not claim validated completion.
- Domains must stop at a checkpoint and run their owned rollback boundary.
- HTTP 202 from `notifications/cancelled` proves only notification acceptance; it
  does not prove UEREMCP work stopped.

WS-13 should update job guides to remove “transport cancel SKIP residual” wording and
state the permanent protocol limitation separately from supported job cancellation.

## Verification

- Static handoff/drift guard: PASS.
- Isolated `RunUAT BuildPlugin`: PASS, 183/183 actions.
- Isolated editor automation `UEREMCP.Transport`: PASS, 8/8 tests.
- Schema validation and ownership checks are recorded in the branch closeout.

