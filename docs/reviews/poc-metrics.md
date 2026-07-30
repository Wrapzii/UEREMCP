# POC metrics

**Owner:** WS-14  
**Status:** Partial — POC-B client wall measured via editor equivalent; tokens / primitive baseline honestly unavailable  
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
**Production B10 lineage:** material `268a102`; pass record `6cc1b7a`; screenshot evidence `87d6c81` (`warm_changed_pixels=30454`)  
**Handoffs:** `docs/proposals/ws-11-pocb-b1-b10-metrics-handoff.md` (git `41f4f82` / `7b654f4` lineage),
`docs/proposals/ws-07-poc-b-primitive-baseline-sequence.md` (git `f295eb8`)

| Field | Value | Status | Evidence |
|---|---|---|---|
| `mcp_round_trips` | **1** | measured | MCP response metrics on successful one-call create |
| `internal_operations` | **46** | measured | same response |
| `server_side_lower_bound_seconds` | **2.319** | measured (≠ wall clock) | Editor log dispatch `11:14:53.492` → content-validation `11:14:55.811` on `RE-backup-2026.07.30-11.18.22.log` lines 3034–3075 per WS-11. **Raw log file is no longer present under `RE/Saved/Logs/` as of 2026-07-30 WS-14 inspection**; value retained as cited `[VERIFIED-RUNTIME]` from WS-11 handoff, reproduced by harness fixture `metrics/fixtures/sample_editor_interval.log` |
| `wall_clock_seconds` | **31.370670** | measured — editor single-call equivalent | `metrics/artifacts/poc_b_editor_single_call_20260730_095849.json`; client `System.Diagnostics.Stopwatch` immediately around one `tests/run_poc_b_fireball.ps1` invocation. Includes cold editor startup, the one goal-level create + validation, and shutdown. Live MCP was attempted first and refused before dispatch (`WinError 10061`), so this uses the explicitly permitted editor equivalent; it is not labeled MCP transport timing. |
| `tokens_total` | — | **unavailable** | Cursor MCP caller exposes no per-call agent usage; `wire_bytes/4` is not total agent tokens. Reason string: see `metrics/token_accounting.py` `CURSOR_MCP_NO_USAGE` |
| `primitive_call_equivalent` | — | **unavailable** | Live discovery confirmed the named Epic tools, but WS-07's sequence is not executable without inventing emitter template refs, Niagara variable type/default payloads, renderer payloads, a fixed material primitive chain, and compile/save policy. Exact attempt record: `metrics/artifacts/poc_b_primitive_baseline_attempt_20260730.json`. The planned 17 and UEREMCP 46 are not substituted. |
| Planned known Niagara min (excl. materials + compile polls) | **17** | planned_partial only | `metrics/primitive_baseline.py` from WS-07 sequence — **not** a measured baseline |
| Completion (overall POC B) | not claimed | open | Production B10 now PASSes on `268a102` / `6cc1b7a` with `warm_changed_pixels=30454`. Overall remains orchestration-owned and blocked by the current-lineage B1–B10/B8 evidence bundle plus the unavailable primitive baseline. |

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

### Live-run outcome and blockers

1. **Client wall clock** — closed with the permitted editor single-call equivalent: **31.370670s**. The live MCP endpoint was tried first and actively refused the connection before dispatch; evidence records the transport distinction and cold-start scope.  
2. **Total agent tokens** — requires a harness that reports input/output usage for an isolated goal trial; Cursor MCP does not.  
3. **Primitive baseline wall clock + ops** — unavailable from the current WS-07 outline. It names operations but does not fix the concrete emitter, variable, renderer, material, compile-poll, or save inputs needed for a semantically equivalent executable script. WS-07 must supply that fixture; then WS-11/WS-14 can run the requested clean-state trials. Planned known minimum **17** remains non-measured.  
4. **Raw B1 log archival** — restore or re-capture `RE-backup-2026.07.30-11.18.22.log` (or a new timed run) for independent re-parse.  
5. **Overall POC-B lineage** — B10 is closed on `268a102` / `6cc1b7a`, but orchestration still needs one complete current-lineage B1–B10/B8 evidence bundle. WS-14 does not claim overall POC B.

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
