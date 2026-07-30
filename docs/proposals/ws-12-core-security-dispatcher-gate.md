# Proposal: Integrate the security gate in Core dispatch

- **From:** WS-12
- **To:** WS-03 (`Plugins/UEREMCP/Source/UeremcpCore/**`)
- **Date:** 2026-07-30
- **Status:** Open
- **Related:** ADR-0010 §§3–4, ADR-0009, `docs/SECURITY.md`

## Ownership blocker

WS-12 owns `Plugins/UEREMCP/Source/UeremcpSecurity/**` and `docs/SECURITY.md`;
WS-03 owns `Plugins/UEREMCP/Source/UeremcpCore/**`
`[VERIFIED: docs/WORK_ALLOCATION.md:26,35]`. WS-12 therefore did not edit Core.

The current Core reference toolset parses envelopes and directly constructs a
response; it has no shared pre-domain security dispatch gate
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpReferenceToolset.cpp:18-51]`.
Its build rules depend on Protocol but not Security
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/UeremcpCore.Build.cs:18-31]`.

## Ask

Add one Core-owned dispatch wrapper used by every mutating domain entry point before
calling its service. The wrapper must:

1. Parse the envelope through `FUeremcpEnvelope::ParseRequest`.
2. Build `FUeremcpPermissionOptions`, preserving whether `options.dry_run` was
   explicitly present. The parsed `FUeremcpRequest::bDryRun` value alone does not
   preserve presence
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Public/UeremcpEnvelope.h:63-78]`.
3. Call `FUeremcpPermissionPolicy::Evaluate` and reject a denied verdict before
   domain work
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Public/UeremcpPermissionPolicy.h:11-31]`.
4. Validate `request.project.path`, target soft paths, and any filesystem paths before
   sandbox entry
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Public/UeremcpPathPolicy.h:11-45]`.
5. For `write|destructive|unsafe`, call
   `FUeremcpMutatorQueue::TryAcquire(ProjectKey, RequestId, Tier)`. If queued, return
   `partially_completed` with the returned stable `JobId`; polling retries the same
   acquire until FIFO head. Cooperative job cancellation calls `CancelQueued`.
   `read` bypasses the queue
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Public/UeremcpMutatorQueue.h]`.
6. Hold queue ownership through sandbox resolution, verification, response
   construction, and audit append. Release on every terminal path using an RAII guard.
7. Append one `FUeremcpAuditRecord` for terminal success, rejection after parsing,
   rollback, failed validation, and destructive dry-run discard. Audit failure must
   be surfaced in diagnostics/capability notes rather than silently discarded
   `[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Public/UeremcpAuditLog.h]`.

## Required build change

Add `"UeremcpSecurity"` to Core's private dependencies. Do not add a dependency from
Security back to Core; that would create a module cycle.

## Queue/job integration constraint

The queue returns a stable job id but intentionally does not own transport polling.
Core/Transport maps it to ADR-0009's `partially_completed` response and job registry.
Do not promote a waiter automatically on release: the FIFO head claims the slot on
its next poll, avoiding an abandoned waiter holding the active slot.

## Acceptance

- `UEREMCP.Security.MutatorQueue.SerializesMutators` passes.
- `UEREMCP.Security.Audit.AppendOnlyJsonl` passes.
- A Core/domain integration test proves two concurrent mutators do not overlap.
- A destructive dry run writes an audit line after discard.
- Every acquired slot is released on success, rejection, failure, and exception paths.
- `python tools/check_ownership.py --ws WS-03` passes for the Core change.

## Limitation

No common mutating dispatcher exists in the synced tree, so WS-12 cannot demonstrate
end-to-end enforcement until WS-03 adds this hook and each domain routes mutations
through it. The owned queue and audit implementations are complete, but system-wide
security enforcement remains **partially completed**.
