# Proposal: ADR-0010 — Security & reliability model (WS-12 → WS-01)

- **From:** WS-12
- **To:** WS-01 (owns ADRs)
- **Date:** 2026-07-29
- **Related:** ADR-0002, ADR-0003, ADR-0005, ADR-0006, pending ADR-0009 (WS-04 job model),
  RB-13, WS-04 concurrent-clients + modal proposals, WS-11 sandbox semantics proposal
- **Status:** ready for ADR drafting — research complete; no implementation yet
- **Evidence:** `docs/research/RB-13-security.md`

## Ask

Accept an **ADR-0010: Security, permissions, and reliability boundaries** with the
decisions below. Do **not** redesign ADR-0002/0005/0006; this ADR layers application
controls Epic's MCP substrate does not provide.

## Context (one paragraph)

Epic's in-process MCP server (ADR-0002) has **no authentication**, an **Origin-only**
DNS-rebinding guard, and **loopback bind by default**. Concurrent sessions are allowed
without a mutator lock. `FileSandbox` is a **transaction** boundary (content mounts
only), not an ACL. Agents can already reach delete/write/`execute_tool_script` via Epic
toolsets. The dominant risk is agent error at speed, not remote attackers on a
correctly loopback-bound editor. Full citations: RB-13.

## Proposed decisions

### 1. Threat model

ADR-0010 centres **agent error / swarm misuse**. Network attack is mitigated by
**requiring loopback bind**; UEREMCP does not fork Epic MCP to add tokens (ADR-0002).

### 2. Permission tiers

| Tier | Default | Elevates via |
|---|---|---|
| `read` | on | — |
| `write` | **on** (UEREMCP default) | — |
| `destructive` | **off** until request opts in | explicit `mode` ∈ {`delete`,`replace`} or `options.allow_destructive: true` |
| `unsafe` | **off** | project `UUeremcpSecuritySettings` only; request cannot self-elevate |

`ProgrammaticToolset.execute_tool_script` and any console/OS escape hatches are
`unsafe`. UEREMCP v1 does **not** wrap them; if exposed later, name-filter blocked
unless settings enable.

### 3. Enforcement points (ordered)

1. **Settings / tool filters** — `SetNameFilters` hide unsafe tools.
2. **Envelope gate** (`UeremcpSecurity` policy) — action × mode × tier × `dry_run`.
3. **Path validator** — soft paths + filesystem realpath; reject `/Engine/` writes,
   traversal, cross-project mismatch (`request.project.path` ≠ open `.uproject`).
4. **Mutator queue** — single active `write|destructive|unsafe` op per open project;
   `read` concurrent. Compose with ADR-0009 poll/job when accepted (WS-04 handoff).
5. **Sandbox hygiene** — if `FGlobalSandbox::IsActive()` under another name, **reject**;
   never Leave another's sandbox (WS-11 hazard).
6. **Audit JSONL** — append to `Saved/UEREMCP/audit/` (outside FileSandbox mounts).

### 4. Destructive `dry_run` override

Keep envelope schema default `dry_run: false` for non-destructive create/update
(ADR-0003). Policy forces `dry_run: true` unless the request **explicitly** sets
`dry_run: false` when:

- `mode` is `delete`, or
- `mode` is `replace` and target exists, or
- predicted `deleted_assets` non-empty.

Document the table in ADR-0010 and implement once in `UeremcpSecurity` (domains call
it; they do not fork it).

### 5. Binding & auto-start

- **Recommend / document:** loopback only (`127.0.0.1` / `localhost`). Runtime on RE
  observed `127.0.0.1:8000` LISTENING.
- **Warn or refuse mutators** if bind is detected as non-loopback (best-effort).
- **`bAutoStartServer`:** project may set `true` for agent workflows (RE already does).
  Engine default `false` remains correct. UEREMCP does not start Epic's server itself
  (WS-04 C14).

### 6. Idempotency / audit writable root

Coordinate with WS-05: persist idempotency + audit under **`Saved/UEREMCP/`**, not
under `Intermediate/Sandboxes/`, so `Discard()` cannot erase safety records
(`ISandboxInstance` excludes Saved/Config).

### 7. UndoTransaction

**Forbidden** as an agent-facing action. Internal use only when the same stack owns the
transaction delta (`GetActiveUndoCount` bracketing). Prefer sandbox `Discard()` for
rollback (ADR-0005).

### 8. Modal / UnrealWatchMCP

Document as **required companion** for unattended swarms; out-of-process; never
auto-dismiss destructive dialogs. Not part of in-process UEREMCP transport (WS-04).

### 9. Source control

Git on RE is the human safety net. Unreal SCC checkout is optional enhancement, not a
POC gate.

## Concrete Wave 2 implementation plan (`UeremcpSecurity`)

| Step | Deliverable | Depends |
|---|---|---|
| 1 | Module skeleton + `UUeremcpSecuritySettings` (tiers, allow_unsafe, audit retention) | WS-03 uplugin registration proposal |
| 2 | `FUeremcpPathPolicy::ValidateSoftPath` / `ValidateFilesystemPath` + unit tests | WS-05 envelope parse |
| 3 | `FUeremcpPermissionPolicy::Evaluate(action, mode, options, target_exists)` | schemas |
| 4 | `FUeremcpMutatorQueue` (game-thread aware; job handoff per ADR-0009) | WS-04 constraints JSON |
| 5 | `FUeremcpAuditLog` JSONL writer under `Saved/UEREMCP/audit/` | path policy |
| 6 | Wire gate into domain tool entry (UeremcpCore dispatcher) | WS-03 |
| 7 | Integration tests: reject `/Engine/` write, reject evil path traversal, destructive
    dry-run default, mutator queue serializes two writers, audit line written on
    dry_run discard | WS-11 harness |
| 8 | Publish `docs/SECURITY.md` (operator + agent guide) from this ADR | ADR accepted |

**Out of scope for v1:** transport Bearer auth, LAN bind support, auto-dismiss modals,
Perforce-required workflows.

## Alternatives considered

| Alternative | Why reject |
|---|---|
| Fork Epic MCP to add auth | Violates ADR-0002; high maintenance |
| External reverse-proxy auth only | Fine as operator add-on; not a substitute for path/tier/dry-run |
| Rely on FileSandbox as ACL | Explicitly not; Saved/Config + Leave hazards |
| Soft last-writer-wins for swarm | Violates ADR-0006 spirit; queue instead |
| Default all ops `dry_run: true` | Breaks agent efficiency cost model (WHY.md); override only destructive |

## High risks if ADR delayed

1. Domain WSs ship mutators with no path gate → `/Engine/` or cross-project writes.
2. Destructive modes inherit `dry_run: false` → silent user-content loss.
3. Parallel agents + sandbox Leave → corrupted Intermediate/Sandboxes + surprise Persist.
4. Someone wraps `execute_tool_script` as a "batch helper" without `unsafe` off.

## Verification tags for ADR drafting

Prefer citing RB-13 sections rather than re-deriving. Key anchors:

- Origin-only auth + runtime 403/200/Bearer-ignored
- `127.0.0.1:8000` LISTENING; HttpServer default `localhost`
- `ISandboxInstance.h:28-30` Saved/Config exclusion
- `UndoTransaction` → `GEditor->UndoTransaction`
- Envelope `dry_run` default false
- WS-11 DiscardFiles / Leave hazards
- WS-04 concurrent clients + modal companion

## No ownership violation

This file is a proposal. ADR text itself remains WS-01. `docs/SECURITY.md` and
`Plugins/.../UeremcpSecurity/**` remain WS-12 after ADR acceptance.

## Response (WS-01)

**Accepted — 2026-07-29.** Frozen as
[`docs/adr/ADR-0010-security-reliability.md`](../adr/ADR-0010-security-reliability.md).

Wave 2 implementation of `UeremcpSecurity` + `docs/SECURITY.md` proceeds against
that ADR. Do not start until Phase 1 exit (R-01/R-03/R-04/R-06) unless building
pure unit-testable policy helpers that cannot mutate the project.

Idempotency durable root accepted via ADR-0010 §6 /
`ws-12-idempotency-store-root.md` (also noted on ADR-0006).
