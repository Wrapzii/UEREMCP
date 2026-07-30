# POC metrics

**Owner:** WS-14  
**Status:** Partial — POC-B measured slice recorded; tokens / client wall / primitive timing **OPEN**  
**Last updated:** 2026-07-30  
**Harness:** [`metrics/`](metrics/) (parser + prepare script + unit tests)

Do **not** treat this file as metrics-complete. E7 requires measured values for every
POC; open cells below are honest blockers, not zero.

---

## Acceptance formula (no invented thresholds)

From [`docs/POC_ACCEPTANCE.md`](../POC_ACCEPTANCE.md) global rules:

| Required evidence | Source |
|---|---|
| `metrics.mcp_round_trips` | envelope (ADR-0003) |
| `metrics.internal_operations` | envelope (ADR-0003) |
| total tokens | harness usage (POC global rules) |
| wall-clock time | **client** monotonic around the goal (POC global rules) |
| equivalent primitive-call count | baseline arm that replaces ~5:1 REAgentTools comparison (`docs/WHY.md`) |

From [`docs/WHY.md`](../WHY.md) “Measure it” — three numbers per scenario:

1. tool calls to complete the goal  
2. total tokens to complete the goal, including failures and retries  
3. **completion rate** — verified goal achievement  

There is **no numeric pass threshold** in POC_ACCEPTANCE for tokens or wall clock —
only that the numbers are **reported** (not adjectives) and that the primitive
equivalent is stated so reduction is measurable. Metrics completion therefore means
all required cells are **measured** (or tokens honestly `unavailable` with a precise
machine-checkable reason). Claiming completion while wall clock or primitive timing
remain estimated is forbidden.

`metrics.timing_ms.server_total` / editor-log dispatch→completion intervals are
**server-side lower bounds**, never `wall_clock_seconds`.
[VERIFIED: `docs/proposals/ws-11-pocb-b1-b10-metrics-handoff.md`;
`docs/proposals/ws-07-poc-b-primitive-baseline-sequence.md`]

---

## POC B — Niagara fireball (`create_niagara_effect`)

**Canonical request:** `schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json`  
**Orch tip of measured MCP B1/B6 run:** `d07f8f1` (recorded on ws01 lineage; see WS-11 handoff)  
**Handoffs:** `docs/proposals/ws-11-pocb-b1-b10-metrics-handoff.md` (git `41f4f82` / `7b654f4` lineage),
`docs/proposals/ws-07-poc-b-primitive-baseline-sequence.md` (git `f295eb8`)

| Field | Value | Status | Evidence |
|---|---|---|---|
| `mcp_round_trips` | **1** | measured | MCP response metrics on successful one-call create |
| `internal_operations` | **46** | measured | same response |
| `server_side_lower_bound_seconds` | **2.319** | measured (≠ wall clock) | Editor log dispatch `11:14:53.492` → content-validation `11:14:55.811` on `RE-backup-2026.07.30-11.18.22.log` lines 3034–3075 per WS-11. **Raw log file is no longer present under `RE/Saved/Logs/` as of 2026-07-30 WS-14 inspection**; value retained as cited `[VERIFIED-RUNTIME]` from WS-11 handoff, reproduced by harness fixture `metrics/fixtures/sample_editor_interval.log` |
| `wall_clock_seconds` | — | **unavailable / OPEN** | Client monotonic start/end were not captured on the B1 run; must not copy 2.319 |
| `tokens_total` | — | **unavailable** | Cursor MCP caller exposes no per-call agent usage; `wire_bytes/4` is not total agent tokens. Reason string: see `metrics/token_accounting.py` `CURSOR_MCP_NO_USAGE` |
| `primitive_call_equivalent` | — | **OPEN** | No measured fireball-equivalent Epic/REAgentTools trial yet |
| Planned known Niagara min (excl. materials + compile polls) | **17** | planned_partial only | `metrics/primitive_baseline.py` from WS-07 sequence — **not** a measured baseline |
| Completion (overall POC B) | not claimed | open | B10 visible warm signature still FAIL on production fireball; metrics incomplete |

### Comparability audit: `internal_operations=46`

| Question | Answer |
|---|---|
| Comparable to `mcp_round_trips`? | **No.** Different axes; the ratio is the point. |
| Directly substitutable for Epic primitive baseline count? | **No.** UEREMCP increments its own create/bind/material/inspect grain; WS-07 baseline counts Epic `call_tool` steps inside `execute_tool_script` plus material-chain primitives. |
| May cite 46 as “calls replaced”? | **No.** Cite `mcp_round_trips=1` vs measured baseline MCP hops; treat 46 as domain-internal only until a grain-matched trial exists. |

Harness encoding: `metrics/primitive_baseline.comparability_audit()`.

### Token applicability (precise unavailable)

```
unavailable (Cursor MCP caller does not expose per-call agent usage; wire_bytes/4 is a payload-token proxy only, not total agent tokens)
```

[VERIFIED: `REAgentTools/Docs/BENCHMARK_REPORT.md` — Cursor Usage not verified by HTTP MCP script]

### Live-run blockers (remaining)

1. **Client wall clock** — requires coordinated instrumented MCP call (`metrics/run_poc_b_metrics.py --execute --allow-live` after junction clearance). Prepared call is written by the script without executing.  
2. **Total agent tokens** — requires a harness that reports input/output usage for an isolated goal trial; Cursor MCP does not.  
3. **Primitive baseline wall clock + ops** — WS-07 sequence is defined; WS-11 must run ≥3 clean-state Epic/REAgentTools trials without contending with the shared ws01 editor. Planned known minimum **17** excludes OPEN materials + compile-poll counts.  
4. **Raw B1 log archival** — restore or re-capture `RE-backup-2026.07.30-11.18.22.log` (or a new timed run) for independent re-parse.  
5. **B10** — not a metrics cell, but blocks overall POC-B claim while WS-07 fixes visible output.

Prepared handoff: [`docs/proposals/ws-14-poc-b-metrics-live-run-handoff.md`](../proposals/ws-14-poc-b-metrics-live-run-handoff.md).

---

## POC A / C / D / E

| POC | Metrics record | Notes |
|---|---|---|
| A | OPEN | CRT claimed on other tips; A9 MCP metrics / tokens / wall not recorded here yet |
| C | OPEN | not started |
| D | OPEN | not started |
| E7 | Partial | this file exists; POC-B cells incomplete |

---

## How to re-score offline

```powershell
cd $UEREMCP_ROOT-ws14
python docs/reviews/metrics/test_metrics_harness.py
python docs/reviews/metrics/run_poc_b_metrics.py --response docs/reviews/metrics/fixtures/sample_poc_b_response.json
```
