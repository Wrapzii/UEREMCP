# WS-01 canonical POC B acceptance claim

- **Decision owner:** WS-01
- **Claim tip:** `2aab525f3496465c9a4f62dd290ce5a31dd755d6`
- **Criterion bundle tip:** `e85da3ea36c92dd8f2f61c6d29591f53094f9c63`
- **Bundle refresh commit:** `d033dd8`
- **Live MCP measurement commit:** `fca736f`
- **Successful primitive-metrics commit:** `2aab525`
- **Date:** 2026-07-30

## Claim

> **Overall POC B CLAIMED** for the demonstrated goal-level Niagara fireball
> scenario: all binary criteria B1–B10 in `docs/POC_ACCEPTANCE.md` pass on the
> assembled current lineage. One live MCP request completed the goal-level pipeline
> in 4.7001219 seconds, and the measured successful primitive equivalent required
> 63 operations in each of 3/3 trials. The live response status remains honestly
> `partially_completed`; this is a product-status residual, not an invented
> `*_validated` result.

This claim is limited to POC B. It does **not** claim POC C, POC D, or POC E.

## Binary criterion evidence

Canonical assembled bundle:
[`tests/integration/_logs/poc_b_current_lineage_e85da3e.json`](../../tests/integration/_logs/poc_b_current_lineage_e85da3e.json).

| Criterion | Result | Evidence pointer |
|---|---:|---|
| B1 | **PASS** | Bundle `criteria.B1`; live artifact `docs/reviews/metrics/artifacts/poc_b_b1_live_mcp_20260730.json`: one Streamable HTTP MCP request, `B1_single_request_complete=true`, `4.7001219s` |
| B2 | **PASS** | Bundle `criteria.B2`; `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_100132.log` |
| B3 | **PASS** | Bundle `criteria.B3`; same fresh-create log, marker `UEREMCP_POC_B_FIREBALL_GATES B3=PASS` |
| B4 | **PASS** | Bundle `criteria.B4`; same fresh-create log, renderer material bindings verified |
| B5 | **PASS** | Bundle `criteria.B5`; same fresh-create log, marker `UEREMCP_POC_B_FIREBALL_GATES B5=PASS` |
| B6 | **PASS** | Bundle `criteria.B6`; same fresh-create log, marker `UEREMCP_POC_B_FIREBALL_GATES B6=PASS` |
| B7 | **PASS** | Bundle `criteria.B7`; `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_SixEmitterGateScaffold_20260730_100207.log` |
| B8 | **PASS** | Bundle `criteria.B8`; `tests/integration/_logs/poc_b8_current_lineage_87d6c81.json` plus separate restart create/verify logs |
| B9 | **PASS** | Bundle `criteria.B9`; fresh-create marker `B9_present=PASS B9_complete=PASS` |
| B10 | **PASS** | Bundle `criteria.B10`; `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_095353.log`, programmatic gate `warm_changed_pixels=30454`; screenshot supplementary only |

The B1 response status is `partially_completed`. B1–B10 do not require a particular
response status string, so this does not negate those binary results. It does prevent
WS-01 from representing the product response itself as `created_and_validated`.

## Metrics and reduction

Canonical metrics record:
[`docs/reviews/poc-metrics.md`](../reviews/poc-metrics.md) at `2aab525`.

- `mcp_round_trips=1`
- `internal_operations=46` (domain-internal work, not calls replaced)
- live MCP caller wall clock: `4.7001219s`
- `primitive_call_equivalent=63` operations per successful trial
- primitive trials: 3 attempted / 3 usable, all `created_and_validated`
- primitive wall clocks: `6.4162721s`, `6.2031326s`, `6.2120595s`
- agent-facing call-count reduction: **63:1**, or **98.41% fewer calls**
- comparison: 63:1 exceeds the approximately 5:1 baseline thesis in
  `docs/WHY.md`
- tokens: **unavailable** — the Cursor MCP caller exposes no per-call agent usage;
  `wire_bytes/4` is only a payload proxy and must not be reported as total tokens

## Open residuals

1. **Validated status ceiling (WS-07).** The live create response remains
   `partially_completed`, not `created_and_validated`. When structural, compile,
   material, and visible-render gates all pass, the implementation should return
   `created_and_validated`; see
   [`ws-01-niagara-created-and-validated-handoff.md`](./ws-01-niagara-created-and-validated-handoff.md).
2. **Token accounting.** Total agent tokens remain unavailable for the precise
   Cursor caller limitation above. No estimate is substituted.
3. **Quarantine cleanup.** Five older loaded scratch systems may remain under
   `/Game/__UeremcpPoc/__BenchmarkCleanup/`. Repeated live deletion returned
   `false`; safe cleanup may require editor unload/restart.

## WS-11 bundle refresh request

`tests/**` is WS-11-owned. When convenient, refresh
`tests/integration/_logs/poc_b_current_lineage_e85da3e.json` (or write its successor)
to set `overall_poc_b_claimed=true`, point at this WS-01 decision, and update its
metrics snapshot to the `2aab525` values (`internal_operations=46`,
`primitive_call_equivalent=63`, 3/3 usable trials). Preserve
`response_status=partially_completed`, unavailable tokens, and the quarantine
residual exactly; do not relabel the live response as validated.
