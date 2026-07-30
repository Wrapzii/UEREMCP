# WS-01 acceptance-gap audit (2026-07-30)

**Owner:** WS-01. **Status:** evidence snapshot — **not** an overall POC-B claim.

**Explore tip cited:** `7b654f4` (`ws-11-poc-b10-render`).
**This worktree at write time:** `8d74694` on branch `ws-01-acceptance-gap-audit-2026-07-30` (ancestor of `7b654f4`), with uncommitted WS-07 Niagara WIP left untouched.

No overall POC-B claim. No claim that production fireball visibility is closed.

---

## Prioritized summary

### P0 — blocks POC-B

| # | Gap | Owner | Parallel? | Evidence |
|---|---|---|---|---|
| 1 | **B10 production fireball:** 185 particles, **0 warm pixels** | WS-07 primary; WS-08 only if materials prove invisible | No (blocks B) | VisibleRender log `081341`; artifact `poc_b10_fireball.png`. Harness canary **PASS** on `0049153`; production **FAIL**. |
| 2 | **Global metrics incomplete:** `docs/reviews/poc-metrics.md` absent; tokens / wall-clock / primitive baseline missing | WS-11 / WS-14 | Yes | Catalog / WHY require measured reduction; numbers not recorded in a single review artifact. |
| 3 | **No single current-lineage end-to-end proof bundle** tying MCP B1/B6 + editor B2–B9 + B8 + B10 PASS | WS-11 + WS-07 | Partial | Criteria exist across commits/logs; no one tip proves the full B chain green. |
| 4 | **MCP create still returns `partially_completed`** until B10 + metrics close | WS-07 | No (honest status) | Create path must not claim `*_validated` without visibility + metrics close. |

### P1 — after POC-B (do not pretend these are closed)

| Gap | Notes |
|---|---|
| POC E | Open |
| ADR-0006 domain idempotency | Not proven on real domain pipelines (scratch harness ≠ production path) |
| ADR-0005 domain rollback | Unproven on real domain mutators |
| ADR-0010 security | Not wired onto Niagara / Material mutate paths |
| ADR-0009 transport timeout / cancel | SKIP residuals remain |
| `execute_plan` | Internal / template-delegated; **not** agent-facing `AICallable` |
| POC C / D | Not started |
| WS-13 guide | Placeholders |
| `CAPABILITY_CATALOG.md` | Was stale (Phase 0) — refreshed in companion commit |

### P2 / out of POC A–B scope (do not delay POC-B)

POC C/D/E full · Wave 3 breadth · `list_domains` · Phase 5 · ADR-0011 · `repair_blueprint` · complex patch · MF Phase C · headline multi-domain spell.

---

## POC A (honest)

- **Supported:** scoped Complete Round Trip A1–A11 **PASS** on tip `3756244`.
- **Not claimed:** arbitrary complex graphs.
- **Unimplemented:** patch mode (`modify` / A8 escape hatch).
- **Metrics caveat:** `tokens_total=0` on recorded CRT runs — token accounting not closed.

Overall POC A may be claimed for the **scoped** CRT surface only. That does **not** imply POC-B.

---

## POC B (honest — incomplete)

| Criterion | Current reading |
|---|---|
| B1 / B6 | MCP one-shot create + compile awaited — PASS on post-`d07f8f1` lineage (structural) |
| B2–B9 | Editor structural / restart proofs — PASS as structural proofs on recorded tips |
| B10 | Harness canary PASS on `0049153`; **production fireball FAIL** (0 warm pixels / log `081341`) |
| Metrics | Incomplete — see P0 #2 |
| MCP status | Create remains `partially_completed` until B10 + metrics close |

**Verdict:** POC-B is **not** met. Do not claim overall POC-B.

---

## Stale claims table

| Claim / location | Why stale | Correct reading |
|---|---|---|
| `CAPABILITY_CATALOG.md` “Nothing is `available` yet — Phase 0” | Multiple domain toolsets are registered and editor/MCP-tested | Statuses are `partial` / `available` per action; Phase 0 line removed |
| `AGENTS.md` “ADRs 0007 and 0010 remain unwritten” | Both files exist with **Status: Accepted** | Frozen summary lists 0007 and 0010 |
| ADR-0006 overclaim vs scratch harness | Idempotency proven in harness ≠ domain pipelines | Domain idempotency remains P1 |
| `create_spell` header “no mutation” | Easy to misread as “done / validated spell” | Tool is **preflight-only**; still no asset mutation — header must stay honest, not be read as POC D done |
| Residual-plan B10 “compile-blocked” text | B10 compile fixed; gate has executed | B10 **ran**; production FAIL is visibility / warm-signature, not compile |

---

## Tip / lineage note

| Ref | Meaning |
|---|---|
| `7b654f4` | Explore / `ws-11-poc-b10-render` tip cited by this audit |
| `0049153` | B10 harness canary PASS (observe simulated particles) |
| `3756244` | Scoped CRT A1–A11 PASS (POC A) |
| `8d74694` | This worktree base at audit write (earlier than `7b654f4`); may carry WS-07 WIP uncommitted |

See also: `docs/proposals/ws-01-editor-filter-results.md` (pointer section appended 2026-07-30).

---

## Ownership

| Path | WS |
|---|---|
| This file | WS-01 (`docs/proposals/ws-01-*`) |
| Catalog / `AGENTS.md` companion edits | WS-01 |
| B10 visibility fix | WS-07 (materials only if proven invisible → WS-08) |
| Metrics bundle | WS-11 / WS-14 |
| Niagara source WIP in this worktree | **Do not touch** (WS-07) |
