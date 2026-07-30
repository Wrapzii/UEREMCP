# POC metrics

**Owner:** WS-14  
**Status:** E7 rows recorded for A/B/C/D/E (measured or `unavailable` with machine-checkable reason). Overall POC E remains **not** claimed.  
**Last updated:** 2026-07-30  
**Harness:** [`metrics/`](metrics/) (parser + prepare script + unit tests)  
**WS-11 evidence JSON:** `tests/integration/_logs/poc_e7_metrics_20260730.json`

E7 means required cells are **reported** (measured or precisely `unavailable`). It does
**not** mean overall POC E passes.

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
**Primitive fixture measured tip:** `24fbe95`; inherited `Minimal` emitter removed before six role emitters are added, with live `explicitMat` mesh-binding wire casing  
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
| `primitive_call_equivalent` | **63 operations per successful trial** | measured | `metrics/artifacts/poc_b_primitive_baseline_fixed_20260730.json`; all three clean trials returned `status=created_and_validated`, `completed=true`, and `primitive_ops_executed=63`, with every trace operation successful. Against UEREMCP B1's one MCP goal call, this is **63:1**, or **98.41% fewer agent-facing calls**. |
| Primitive wall clock | **6.2771547s mean** (median **6.2120595s**) | measured | Three client-monotonic outer-call intervals: **6.4162721s**, **6.2031326s**, **6.2120595s**. UEREMCP B1's live **4.7001219s** was **25.12% faster** than this primitive mean (**1.34×** speedup). The **31.370670s** editor-equivalent measurement is a distinct cold-start/create/validation/shutdown scope; it is **5.00×** the primitive mean and is not substituted for live transport timing. |
| Primitive baseline trial count | **3 attempted / 3 usable** | passed | Unique JSON-RPC IDs and per-trial script nonces; each trial began with all seven controlled target paths absent. Full traces and raw MCP responses are inline in `metrics/artifacts/poc_b_primitive_baseline_fixed_20260730.json`. |
| Live UEREMCP B1 transport | **PASS, 4.7001219s** | measured | `metrics/artifacts/poc_b_b1_live_mcp_20260730.json`; one uncached Streamable HTTP MCP call using the canonical request with unique request/idempotency identifiers. Response: `metrics.mcp_round_trips=1`, `validation.single_request_pipeline=true`, `poc_b_gates.B1_single_request_complete=true`, status honestly `partially_completed`. |
| Planned known Niagara min (excl. materials + compile polls) | **17** | superseded planning datum | `metrics/primitive_baseline.py` from the earlier WS-07 sequence; not substituted for the measured successful 63-operation executions. |
| Completion (metrics cells) | **closed for POC-B orchestration review** | measured | MCP calls, internal operations, primitive equivalent, primitive and UEREMCP wall clocks, and completion outcomes are measured. Tokens remain precisely unavailable because the Cursor MCP caller exposes no per-call agent usage. This is **not** an overall POC-B claim; orchestration/WS-01 owns that decision and remaining acceptance-lineage review. |

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

1. **Client wall clock** — editor-equivalent goal run remains **31.370670s**. A later live B1 MCP transport trial passed in **4.7001219s**, but its response remains honestly `partially_completed`; the two timings have distinct scopes.  
2. **Total agent tokens** — requires a harness that reports input/output usage for an isolated goal trial; Cursor MCP does not.  
3. **Primitive baseline wall clock + ops — closed.** Three independent clean executions at `24fbe95` all passed: **63 operations** each in **6.4162721s**, **6.2031326s**, and **6.2120595s** (mean **6.2771547s**). UEREMCP B1's one goal call is a **63:1 / 98.41% call-count reduction** and its **4.7001219s** live wall clock is **25.12% faster** than the primitive mean. After the trials, all seven controlled baseline target paths were removed. Five older loaded scratch systems remain quarantined under `/Game/__UeremcpPoc/__BenchmarkCleanup/`; repeated live deletion returned `false`, so deleting them safely requires editor unload/restart.  
4. **Raw B1 log archival** — restore or re-capture `RE-backup-2026.07.30-11.18.22.log` (or a new timed run) for independent re-parse.  
5. **Overall POC-B lineage** — B1 transport passes live, B10 is closed on `268a102` / `6cc1b7a`, and the primitive metrics cells are now complete enough for orchestration's overall-claim review. Create remains honestly `partially_completed`; WS-14 does not claim overall POC B.

Prepared handoff: [`docs/proposals/ws-14-poc-b-metrics-live-run-handoff.md`](../proposals/ws-14-poc-b-metrics-live-run-handoff.md).

---

## POC A — Blueprint complete round-trip

**Evidence:** `tests/integration/_logs/poc_a_complete_round_trip_3756244.json`  
**Tip lineage:** CRT recorded on `3756244` (main/orch claim lineage)

| Field | Value | Status | Evidence |
|---|---|---|---|
| `mcp_round_trips` | **3** | measured | CRT transport evidence JSON |
| `internal_operations` | **4** | measured | same |
| `wall_clock_seconds` | **2.2996132** | measured | client monotonic in CRT script |
| `tokens_total` | — | **unavailable** | Cursor MCP caller exposes no per-call agent usage (`CURSOR_MCP_NO_USAGE`). The CRT artifact's `tokens_total: 0` is **rejected** — zero is not allowed. |
| `primitive_call_equivalent` | **15** | measured | CRT evidence JSON |

---

## POC C — Niagara elemental variation

**Live filter:** `UEREMCP.Niagara.Create.PocCVariationRuntime` (Success on 2026-07-30)  
**Supporting:** `UEREMCP.Templates.POCC.ThirdGeneration` (Success)  
**WS-11 JSON:** `tests/integration/_logs/poc_e7_metrics_20260730.json`

| Field | Value | Status | Evidence |
|---|---|---|---|
| `mcp_round_trips` | **1** | measured | Filter asserts `metrics.mcp_round_trips == 1` in-process; log `editor_UEREMCP_Niagara_Create_PocCVariationRuntime_20260730_124945.log` Result=Success |
| `internal_operations` | — | **unavailable** | Automation filter does not emit envelope `internal_operations` to the editor log |
| `wall_clock_seconds` | — | **unavailable** | No client Stopwatch around the C goal; editor test interval **5.645s** is a server-side lower bound only |
| `tokens_total` | — | **unavailable** | `CURSOR_MCP_NO_USAGE` |
| `primitive_call_equivalent` | — | **unavailable** | No measured REAgentTools/Epic primitive baseline trial for POC C on this tip |

C5 (networking/damage) remains **FAIL** and is outside metrics closure.

---

## POC D — Gameplay create_spell plan

**Live filter:** `UEREMCP.Gameplay.PocD.LiveUpsertViaPlan` (Success on 2026-07-30)  
**WS-11 JSON:** `tests/integration/_logs/poc_e7_metrics_20260730.json`

| Field | Value | Status | Evidence |
|---|---|---|---|
| `mcp_round_trips` | **1** | measured | One `execute_plan` request with one `create_spell` op; log `editor_UEREMCP_Gameplay_PocD_LiveUpsertViaPlan_20260730_125056.log` Result=Success |
| `internal_operations` | — | **unavailable** | Filter does not emit envelope `internal_operations` to the editor log |
| `wall_clock_seconds` | — | **unavailable** | No client Stopwatch; editor test interval **0.017s** is server-side lower bound only |
| `tokens_total` | — | **unavailable** | `CURSOR_MCP_NO_USAGE` |
| `primitive_call_equivalent` | — | **unavailable** | No measured primitive baseline trial for POC D on this tip |

D5 multi-client remains static-only and is outside metrics closure.

---

## POC E — Durability / honesty (not a goal MCP scenario)

| Field | Value | Status | Evidence |
|---|---|---|---|
| `mcp_round_trips` | — | **unavailable** | No single goal-level MCP create; E5/E6/E1 are automation filters |
| `internal_operations` | — | **unavailable** | Honesty/restart filters do not emit goal-level internal ops |
| `wall_clock_seconds` | — | **unavailable** | Scenario E orchestrator client Stopwatch not captured on this closeout |
| `tokens_total` | — | **unavailable** | `CURSOR_MCP_NO_USAGE` |
| `primitive_call_equivalent` | — | **unavailable** | No primitive baseline applies to durability/honesty gates |

Harness evidence: `tests/integration/_logs/poc_e_acceptance_20260730_e7close.json`,
`tests/integration/_logs/poc_e1_ad_restart_clean_20260730.json`.

### E7 rollup

| POC | Metrics cells | Notes |
|---|---|---|
| A | closed | tokens precisely unavailable |
| B | closed | tokens precisely unavailable |
| C | closed | several cells unavailable with reasons |
| D | closed | several cells unavailable with reasons |
| E | closed (harness) | not a goal MCP scenario |
| **Overall POC E** | **not claimed** | E1 full A–D (incl. C5/D5) unmet |

---

## How to re-score offline

```powershell
cd $UEREMCP_ROOT-ws14
python docs/reviews/metrics/test_metrics_harness.py
python docs/reviews/metrics/run_poc_b_metrics.py --response docs/reviews/metrics/fixtures/sample_poc_b_response.json
```
