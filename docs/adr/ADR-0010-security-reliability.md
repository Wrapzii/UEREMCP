# ADR-0010: Security, permissions, and reliability boundaries

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-12 (`UeremcpSecurity`, `docs/SECURITY.md`), every domain mutator
- **Depends on:** ADR-0002, ADR-0003, ADR-0005, ADR-0006, ADR-0009, `RB-13`

## Context

UEREMCP rides Epic's in-process MCP server (ADR-0002). That substrate provides
**no authentication**, an **Origin-only** DNS-rebinding guard, and **loopback bind
by default**. Concurrent MCP sessions are allowed with **no mutator lock**.
`FileSandbox` is a **transaction** boundary over content mounts — it is not an ACL;
`Saved/` and `Config/` are outside it
`[VERIFIED: ISandboxInstance.h:28-30 per RB-13]`.

Runtime on the current machine:

- Origin missing / localhost / `127.0.0.1` → HTTP 200 on `ping`
- `Origin: http://evil.example.com` → HTTP **403**
- `Authorization: Bearer totally-fake` → HTTP **200** (ignored)
- Listener: `127.0.0.1:8000` LISTENING (not `0.0.0.0`)
  `[VERIFIED-RUNTIME: RB-13 A1–A2]`

Epic toolsets already expose delete/write and
`ProgrammaticToolset.execute_tool_script`. The dominant threat is **agent error at
speed** (including concurrent writers), not a remote attacker against a correctly
loopback-bound editor. Full evidence: `docs/research/RB-13-security.md`.

## Decision

We will add **application-layer** controls in `UeremcpSecurity` (Wave 2). We will
**not** fork Epic MCP to add transport tokens (ADR-0002).

### 1. Threat model

Centre **agent / swarm misuse**. Network exposure is mitigated by **requiring
loopback bind** and documenting that any local process can drive the editor.

### 2. Permission tiers

| Tier | Default | Elevates via |
|---|---|---|
| `read` | on | — |
| `write` | **on** | — |
| `destructive` | **off** until request opts in | `mode` ∈ {`delete`,`replace`} or `options.allow_destructive: true` |
| `unsafe` | **off** | project `UUeremcpSecuritySettings` only — request cannot self-elevate |

`ProgrammaticToolset.execute_tool_script` and console/OS escape hatches are
`unsafe`. UEREMCP v1 does **not** wrap them; if exposed later, hide via
`SetNameFilters` unless settings enable.

### 3. Enforcement order

1. Settings / name filters — hide unsafe tools.
2. Envelope gate — `action` × `mode` × tier × `dry_run`.
3. Path validator — soft paths + filesystem realpath; reject `/Engine/` writes,
   traversal, and `request.project.path` ≠ open `.uproject`.
4. Mutator queue — one active `write|destructive|unsafe` op per open project;
   `read` may be concurrent. Compose with ADR-0009 job handles.
5. Sandbox hygiene — if `FGlobalSandbox::IsActive()` under another owner,
   **reject**; never `Leave` a foreign sandbox.
6. Audit JSONL — append under `<ProjectSavedDir>/UEREMCP/audit/` (outside
   FileSandbox content mounts).

### 4. Destructive `dry_run` override

Envelope schema keeps `dry_run` default `false` for ordinary create/update
(ADR-0003 / cost model). Policy **forces** `dry_run: true` unless the request
explicitly sets `dry_run: false` when:

- `mode` is `delete`, or
- `mode` is `replace` and the target exists, or
- predicted `deleted_assets` is non-empty.

Domains call the shared policy; they do not fork it.

### 5. Binding and auto-start

- Recommend loopback only (`127.0.0.1` / `localhost`).
- Warn or refuse mutators if bind is detected as non-loopback (best-effort).
- `bAutoStartServer` may be `true` in agent projects; engine default `false`
  remains correct. UEREMCP does not start Epic's server itself.

### 6. Idempotency and audit writable root

Durable idempotency + audit live under **`Saved/UEREMCP/`**, never under
`Intermediate/Sandboxes/`, so `Discard()` cannot erase safety records. Wave 1
may keep an in-memory session store (ADR-0006); disk under `Saved/UEREMCP/` is
the Wave 2 restart-surviving location
(`docs/proposals/ws-12-idempotency-store-root.md`).

### 7. UndoTransaction

**Forbidden** as an agent-facing action. Internal use only when the same stack
owns the transaction delta. Prefer sandbox `Discard()` for rollback (ADR-0005).

### 8. Modal companion

UnrealWatchMCP / unreal-watch is a **required companion** for unattended swarms:
out-of-process; never auto-dismiss destructive dialogs. Not part of in-process
transport (ADR-0009).

### 9. Source control

Git (or equivalent) is the human safety net. Unreal SCC checkout is optional,
not a POC gate.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Fork Epic MCP to add Bearer/auth | Violates ADR-0002; high maintenance |
| Reverse-proxy auth only | Fine as an operator add-on; not a substitute for path/tier/dry-run |
| Treat FileSandbox as ACL | Explicitly not; Saved/Config + Leave hazards |
| Soft last-writer-wins for swarm | Violates ADR-0006; use mutator queue |
| Default every op `dry_run: true` | Breaks agent efficiency (`docs/WHY.md`); override only destructive |

## Consequences

**Enables:** honest Wave 2 security module; domain WSs share one gate; R-07/R-12/R-13
have a frozen design.

**Costs:** mutator queue serializes writers; destructive ops need an explicit
`dry_run: false`; operators must keep MCP on loopback.

**Locks in:** application-layer controls rather than a forked MCP server; audit/
idempotency root under `Saved/UEREMCP/`.

## Open questions

- Exact JSON schema for `options.allow_destructive` (WS-01 schema bump when
  WS-12 wires the gate).
- Best-effort non-loopback bind detection API surface (RB-13 residual).

## Verification

WS-12 Wave 2 integration tests (via WS-11 harness):

- reject `/Engine/` write and path traversal
- destructive modes default to dry-run unless explicitly overridden
- mutator queue serializes two concurrent writers
- audit line written on dry-run discard
- foreign active sandbox → `rejected`

Operator guide: `docs/SECURITY.md` (WS-12) after module skeleton lands.
