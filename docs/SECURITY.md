# UEREMCP Security Guide

**Owner:** WS-12. **Authority:** [ADR-0010](adr/ADR-0010-security-reliability.md),
[RB-13](research/RB-13-security.md).

UEREMCP adds **application-layer** controls on top of Epic's in-process MCP server.
We do **not** fork Epic MCP for authentication (ADR-0002). The realistic threat is
**agent error at speed**, not a remote attacker on a correctly configured workstation.

## Operator

### Bind loopback only

Epic's MCP server has **no authentication** — only an Origin header guard against
DNS rebinding. Any local process that can open `127.0.0.1:<port>` can drive the full
tool surface.

| Check | Requirement |
|---|---|
| Listener | `127.0.0.1` or `localhost` only — **not** `0.0.0.0` |
| `DefaultBindAddress` | Must **not** be `any` under `[HTTPServer.Listeners]` |
| `.mcp.json` | Point clients at `http://127.0.0.1:<port>/mcp` |

On RE, runtime observed `127.0.0.1:8000` LISTENING `[VERIFIED-RUNTIME: RB-13]`.
If bind is non-loopback, treat the editor as **compromised-capable** until fixed.

`UUeremcpSecuritySettings::bRefuseMutatorsOnNonLoopbackBind` (default **true**) will
refuse UEREMCP mutators when non-loopback bind is detected (best-effort; operator
responsibility remains primary).

### Auto-start

Agent workflows need MCP running unattended. Project-level `bAutoStartServer=True` is
appropriate for RE; engine default `false` remains correct for shipping templates.
UEREMCP does **not** start Epic's server itself.

### Human safety net

Commit to Git (or equivalent) before unattended swarm runs. UEREMCP audit JSONL under
`Saved/UEREMCP/audit/` helps reconstruct what happened; it is **not** undo.

### UnrealWatchMCP companion

For unattended agents, run **[UnrealWatchMCP](https://github.com/REAgentTools/tree/main/Optional/UnrealWatchMCP)**
(`unreal-watch`) alongside the editor. It is **out-of-process**: detects modal dialogs
and game-thread freezes that in-editor MCP cannot self-heal.

- Never auto-dismiss destructive dialogs.
- Do not thrash MCP re-init on `CLOSE_WAIT` — follow UnrealWatch retry policy.

## Agent

### Permission tiers (ADR-0010)

| Tier | Default | Meaning |
|---|---|---|
| `read` | on | Inspect, describe, validate-only |
| `write` | **on** | Create/modify inside allowed roots |
| `destructive` | off until opted in | `delete`, `replace` on existing assets |
| `unsafe` | **off** | `execute_tool_script`, console/OS escape — **project settings only** |

Requests **cannot** elevate to `unsafe`. Only `UUeremcpSecuritySettings::bAllowUnsafe`
can enable it. UEREMCP v1 does **not** wrap `execute_tool_script` as a normal tool.
`FUeremcpPermissionPolicy::IsUnsafeAction` rejects those action names unless settings
allow `[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpPermissionPolicy.cpp]`.

### Destructive `dry_run`

Envelope schema default is `dry_run: false` for ordinary create/update (cost model).
`FUeremcpPermissionPolicy` **forces** `dry_run: true` unless the request **explicitly**
sets `dry_run: false` when:

- `mode` is `delete`, or
- `mode` is `replace` and the target exists, or
- `mode` is `rebuild_from_specification` and the target exists, or
- predicted `deleted_assets` is non-empty.

Predicted deletes outside native `delete`/`replace`/`rebuild_from_specification` modes
also require `options.allow_destructive: true` or the request is **rejected**.

Plan destructive work with a dry run first; opt out only deliberately.

### Allowed paths

`FUeremcpPathPolicy` runs **before** `FileSandbox` Enter:

| Layer | Accept (write) | Reject |
|---|---|---|
| Soft path | `/Game/...`, project plugin roots | `/Engine/` writes, `/Temp/`, `..`, Windows absolutes |
| Filesystem | `Content/`, `Saved/UEREMCP/**` | Outside project tree, `Saved/` except `UEREMCP/` |
| Project | `request.project.path` == open `.uproject` | Mismatch or no project loaded |

`FileSandbox` is rollback, not ACL — `Saved/` and `Config/` are outside mount tracking
`[VERIFIED: ISandboxInstance.h:28-30 per RB-13]`.

### Concurrency

`FUeremcpMutatorQueue` serialises `write|destructive|unsafe` FIFO — one active
mutator per normalized project key. `read` bypasses the queue. A waiter receives a
stable job id and claims the slot by retrying after the prior owner releases it
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpMutatorQueue.cpp]`.
Core maps that id into ADR-0009 `partially_completed` via `FUeremcpMutatingDispatch`.

### Job cancellation

UEREMCP supports cooperative cancellation through the AICallable
`cancel_job(job_id)` action for jobs that advertise `cancellable: true`. The production
scheduler path is editor-verified: the worker observed its cancellation token, ran one
domain rollback checkpoint, stopped before validated completion, and remained pollable
as terminal `job.state: cancelled`
`[VERIFIED-RUNTIME: UEREMCP.Transport.JobRegistry.Cancel,
editor_UEREMCP_Transport_20260730_143347.log, 8/8 Success;
docs/proposals/ws-04-cancellation-hardening-closeout.md:51-68]`.

Cancellation is not a kill primitive. Operators and agents must:

- call `cancel_job` with the UEREMCP `job_id`;
- treat a cancelled job as terminal without validated completion; and
- let each domain stop at a cooperative checkpoint and execute its owned
  FileSandbox/transaction rollback boundary under ADR-0005.

Epic MCP `notifications/cancelled` cannot reach ToolsetRegistry/AICallable work in UE
5.8. Epic's private ToolsetRegistry adapter and tool-search `FCallTool` override
`RunAsync` but not `CancelAsync`
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.h:13-26;
$UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolSearch.h:61-80]`.
This is an immutable UE 5.8 adapter limitation, not an open UEREMCP residual. HTTP 202
for that notification proves only that Epic accepted the notification; it does not
prove the UEREMCP job stopped. Use `cancel_job(job_id)` and poll the retained envelope.

### Forbidden patterns

- Agent-facing `UndoTransaction` — use sandbox `Discard()` (ADR-0005).
- Wrapping Epic `execute_tool_script` without `unsafe` tier + name filters.
- Per-domain path allowlists looser than `FUeremcpPathPolicy`.
- Calling Security primitives piecemeal from domains when `FUeremcpMutatingDispatch`
  already composes them (fork risk).

## Preferred domain gate: `FUeremcpMutatingDispatch`

**Owner:** WS-03 (`UeremcpCore`). **Policy substrate:** WS-12 (`UeremcpSecurity`).

Domains must **not** reimplement permission / path / queue / audit. Call the Core RAII
gate:

```cpp
#include "UeremcpMutatingDispatch.h"  // also add UeremcpSecurity to PrivateDependencyModuleNames

FUeremcpMutatingDispatch Gate;
FString Blocking;
if (!Gate.TryBegin(
	RequestJson,
	bTargetExists,
	PredictedDeletedAssetCount,  // use FUeremcpSecurityDomainAdoption::PredictedDeletedForDestructiveReplace
	bReadOnlyOperation,
	Blocking))
{
	return Blocking;  // rejected or partially_completed (queued)
}

// ... mutate, save, re-read verify ...
return Gate.Complete(Response);  // audit append + mutator release + serialize
```

`[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpMutatingDispatch.h;
Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpMutatingDispatch.cpp]`

| Parameter | Typical create | Inspect / read | Replace existing |
|---|---|---|---|
| `bTargetExists` | asset already on disk | true | true |
| `PredictedDeletedAssetCount` | 0 | 0 | 1 (or helper) |
| `bReadOnlyOperation` | false | **true** | false |

Live mutators that skip `TryBegin` when `options.dry_run` is true (Gameplay /
Blueprint pattern) are acceptable **only** when dry-run paths never acquire the
queue and never write. Prefer always calling `TryBegin` with the effective dry-run
verdict when in doubt.

Helpers for PredictedDeleted / soft-path preflight without forking policy:
`FUeremcpSecurityDomainAdoption` in `UeremcpSecurityDomainAdoption.h`
(umbrella: `UeremcpSecurity.h`).

### Domain adoption status (2026-07-30)

| Domain | Mutating entry | Gate wired? | Notes |
|---|---|---|---|
| Gameplay (WS-09) | `CreateSpell` | **yes** | `FUeremcpMutatingDispatch` around live DataTable write |
| Blueprint (WS-06) | `SubmitGraph` / `ReadGraph` | **yes** | `FUeremcpBlueprintMutatingGate` adapter |
| Niagara (WS-07) | `CreateNiagaraEffect` | **yes** | `FUeremcpMutatingDispatch` + ADR-0006 revision/idempotency gates (2026-07-30) |
| Material (WS-08) | `CreateVfxMaterial`, `CreateProceduralTexture` | **yes** | `FUeremcpMutatingDispatch` (prior) + ADR-0006 revision/idempotency gates (2026-07-30) |

**R-07 mitigated** for live mutators that use `FUeremcpMutatingDispatch`
(Blueprint, Gameplay, Niagara, Material — table above)
`[VERIFIED: docs/proposals/ws-01-poc-closeout-2026-07-30.md;
docs/proposals/ws-12-niagara-mutating-dispatch-handoff.md;
docs/proposals/ws-12-material-mutating-dispatch-handoff.md]`.
**R-07 residual remains** for any mutate path that skips the gate (Animation writes
if added; Templates `promote_to_template`; future domains) and for operator loopback
discipline (R-13). Do not claim universal content-safety.

**R-12 mitigated** for gated mutators: `FUeremcpMutatorQueue::IsImplemented()` is
true and Security automation serialises concurrent writers
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpMutatorQueue.cpp;
UEREMCP.Security.MutatorQueue.SerializesMutators]`. Residual: ungated paths and
tag/INI concurrency outside the queue.

## Audit

Append-only JSONL: `<ProjectSavedDir>/UEREMCP/audit/YYYY-MM-DD.jsonl`.

Retention: `UUeremcpSecuritySettings::AuditRetentionDays` (default **14**).

Fields: timestamp, `request_id`, `idempotency_key`, action, mode, status, targets,
created/modified/deleted lists, `dry_run`, tier, revision before/after, project path
(RB-13 B8).

Writes are process-serialized, directory creation is recursive, each record is
condensed JSON followed by one newline, and file writes use append mode
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpAuditLog.cpp;
FFileHelper.h:196; FileManager.h:20,122]`.

## Module map

| Component | Role |
|---|---|
| `UUeremcpSecuritySettings` | Project tiers, `bAllowUnsafe`, audit retention |
| `FUeremcpPathPolicy` | Soft + filesystem roots |
| `FUeremcpPermissionPolicy` | Tier + destructive dry_run + unsafe action gate |
| `FUeremcpMutatorQueue` | Per-project FIFO single active mutator |
| `FUeremcpAuditLog` | Append-only daily JSONL writer + retention prune |
| `FUeremcpSecurityDomainAdoption` | Domain helpers (PredictedDeleted, soft-path wrap) |
| `FUeremcpMutatingDispatch` | **Core** RAII composition of the above (WS-03) |

### Not a toolset

`UeremcpSecurity` registers **no** `AICallable` tools and does **not** call
`UToolsetRegistry::RegisterToolsetClass`. It is a policy library loaded as a plugin
module; domains and Core consume it
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpSecurityModule.cpp]`.
Registration of the module in `UEREMCP.uplugin` landed in WS-03 merge `9148d52`
`[VERIFIED: docs/proposals/ws-12-register-security-module.md:60-63]`.

Core dispatcher (`FUeremcpMutatingDispatch`) landed per
`docs/proposals/ws-12-core-security-dispatcher-gate.md`. Wired live mutators are
listed in the domain adoption table above. Residual work is **any mutate path that
still skips the gate**, not Core wiring itself.

## Legacy primitive sequence (Core/tests only)

Prefer `FUeremcpMutatingDispatch`. If composing primitives (Core or automation only):

```cpp
// 1) FUeremcpPermissionPolicy::Evaluate(...)
// 2) FUeremcpPathPolicy::ValidateSoftPath / ValidateProjectPathMatch
// 3) FUeremcpMutatorQueue::TryAcquire → Release / CancelQueued
// 4) FUeremcpAuditLog::Append on every terminal path
```

## Tests

```powershell
# Header/docs contract (no Unreal):
python Plugins/UEREMCP/Source/UeremcpSecurity/scripts/test_security_contract.py

# Editor automation (plugin built; does not require Niagara rebuild):
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Security"
```

Coverage includes: `/Engine/` write rejection, destructive dry-run force
(including `rebuild_from_specification`), predicted-delete `allow_destructive`
rejection, unsafe action denial, mutator FIFO serialization, audit JSONL append,
and `FUeremcpSecurityDomainAdoption` helpers.
