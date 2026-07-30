# WS-12 hazard list — silent user-content destruction

- **From:** WS-12
- **To:** all workstreams (especially domain WS-06..10, WS-05 batch, WS-03 core)
- **Date:** 2026-07-29
- **Evidence:** `docs/research/RB-13-security.md`, WS-11 sandbox proposal, WS-04 modal/concurrency proposals
- **Status:** research; binding once ADR-0010 accepted

Treat each row as a **destroy-user-work** bug class (`AGENTS.md` rule 8). Do not
"work around" silently — refuse, dry-run, or escalate.

| ID | Hazard | Do / Don't | Evidence |
|---|---|---|---|
| H1 | `UndoTransaction` undoes **global** stack (may be human edits) | Don't expose to agents; don't undo without owning BeginTransaction delta | `[VERIFIED: ToolsetLibrary.cpp:288-294]` |
| H2 | `mode: delete` / `replace` with envelope `dry_run` default **false** | Force dry-run true unless explicit false | `[VERIFIED: request.schema.json:61-64]` |
| H3 | `FileSandbox` is **not** an ACL; Saved/Config writes persist outside Discard | Don't claim rollback for non-mount paths; put audit/idempotency in `Saved/UEREMCP/` | `[VERIFIED: ISandboxInstance.h:28-30]` |
| H4 | `Enter` different sandbox name **Leaves** prior (files kept) | If `IsActive()` other name → reject; never silent Leave | `[VERIFIED: WS-11 proposal / SandboxLibrary.cpp:67-81]` |
| H5 | `DiscardFiles` without purge/hot-reload → stale UObjects/AR | Prefer full `Discard()` on failure | `[VERIFIED: WS-11 proposal]` |
| H6 | Concurrent MCP sessions + no mutator lock | Don't assume ADR-0006 alone serializes writers; await ADR-0010 queue | `[VERIFIED: ws-04-concurrent-clients.md]` |
| H7 | Modal dialog blocks game thread; in-editor tools cannot dismiss | Use UnrealWatchMCP; don't auto-click destructive buttons | `[VERIFIED: UnrealWatchMCP/README.md]` |
| H8 | `execute_tool_script` can invoke any registered tool (incl. delete) | Treat as `unsafe`; don't wrap in UEREMCP v1 | `[VERIFIED: programmatic.py:906+]` |
| H9 | Asset file tools may include **engine plugin Content** roots | UEREMCP path policy must be tighter than Epic AssetTools defaults | `[VERIFIED: asset.py:565-571]` |
| H10 | MCP has **no auth**; non-loopback bind exposes full tool surface | Keep bind on 127.0.0.1; never recommend `DefaultBindAddress=any` | `[VERIFIED-RUNTIME: netstat + Origin probes]` |
| H11 | Cross-project `project.path` mismatch | Reject if ≠ open `.uproject` | ADR-0010 proposal |
| H12 | Auto-suffix on collision | Forbidden (ADR-0006); creates duplicate sprawl | `[VERIFIED: ADR-0006]` |
| H13 | BP compile / deletion rollback still partial | Don't set `rollback.available: true` beyond WS-11 proven Content/ path | `[VERIFIED: WS-11 proposal]` |
| H14 | Client disconnect may leave tool running; result dropped | Design jobs/idempotency for orphan completion | `[VERIFIED: RB-04 §A7]` |

## Immediate asks of other WSs

- **WS-03:** reserve a dispatcher hook for `UeremcpSecurity` gate before domain services.
- **WS-05:** idempotency store under `Saved/UEREMCP/` (not Intermediate/Sandboxes); destructive dry-run policy table shared.
- **WS-04 / ADR-0009:** mutator queue waiters should use the poll job model.
- **WS-11:** keep Rollback gate honest; add tests for H1/H2/H4 when harness ready.
- **Domain WSs:** do not invent per-domain path allowlists that are looser than WS-12.

## Not asking anyone to implement yet

Phase 1 research only. Implementation owns `Plugins/UEREMCP/Source/UeremcpSecurity/**`
and `docs/SECURITY.md` after ADR-0010 acceptance.
