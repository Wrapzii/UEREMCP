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

### Destructive `dry_run`

Envelope schema default is `dry_run: false` for ordinary create/update (cost model).
`FUeremcpPermissionPolicy` **forces** `dry_run: true` unless the request **explicitly**
sets `dry_run: false` when:

- `mode` is `delete`, or
- `mode` is `replace` and the target exists, or
- `mode` is `rebuild_from_specification` and the target exists, or
- predicted `deleted_assets` is non-empty.

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

`FUeremcpMutatorQueue` (Wave 2) serialises `write|destructive|unsafe` — one active
mutator per project. `read` tools stay concurrent. Waiters compose with ADR-0009 job
poll handles.

### Forbidden patterns

- Agent-facing `UndoTransaction` — use sandbox `Discard()` (ADR-0005).
- Wrapping Epic `execute_tool_script` without `unsafe` tier + name filters.
- Per-domain path allowlists looser than `FUeremcpPathPolicy`.

## Audit

Append-only JSONL: `<ProjectSavedDir>/UEREMCP/audit/YYYY-MM-DD.jsonl`.

Retention: `UUeremcpSecuritySettings::AuditRetentionDays` (default **14**).

Fields: timestamp, `request_id`, `idempotency_key`, action, mode, status, targets,
created/modified/deleted lists, `dry_run`, tier, revision before/after, project path
(RB-13 B8).

## Module map

| Component | Role |
|---|---|
| `UUeremcpSecuritySettings` | Project tiers, `bAllowUnsafe`, audit retention |
| `FUeremcpPathPolicy` | Soft + filesystem roots |
| `FUeremcpPermissionPolicy` | Tier + destructive dry_run gate |
| `FUeremcpMutatorQueue` | Single active writer (stub) |
| `FUeremcpAuditLog` | JSONL writer (stub) |

Registration in `UEREMCP.uplugin` is pending — see
`docs/proposals/ws-12-register-security-module.md` (WS-03).

## Tests

```powershell
# Header/docs contract (no Unreal):
python Plugins/UEREMCP/Source/UeremcpSecurity/scripts/test_security_contract.py

# Editor automation (plugin built):
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Security"
```
