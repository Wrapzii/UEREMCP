# WS-14 → WS-11/WS-07/WS-01: POC-B metrics live-run handoff

**Status:** Harness ready; live editor/MCP execution blocked pending junction coordination  
**Owner (record):** WS-14 (`docs/reviews/poc-metrics.md`, `docs/reviews/metrics/`)  
**Owner (live trials):** WS-11 execution; WS-07 sequence authority; WS-01 junction clearance  
**Date:** 2026-07-30

## What WS-14 completed offline

1. Acceptance formula pinned to `docs/POC_ACCEPTANCE.md` + `docs/WHY.md` (no invented thresholds).
2. Recorded measured POC-B slice in `docs/reviews/poc-metrics.md`:
   - `mcp_round_trips=1`
   - `internal_operations=46`
   - server-side lower bound `2.319s` (explicitly not wall clock)
3. Honest `unavailable` token result with machine-checkable reason (Cursor MCP).
4. Comparability audit: `46` is **not** an Epic primitive baseline and **not** an MCP round-trip count.
5. Reproducible harness under `docs/reviews/metrics/` with unit tests.
6. Prepare-only live script that prints the exact `CreateNiagaraEffect` call and refuses uncoordinated `--execute`.

## Do not contend with ws01

The RE plugin junction is pointed at the active WS-07/ws01 worktree. WS-14 will not:

- switch the junction
- call live MCP against the shared editor
- rebuild plugins

## Exact prepared UEREMCP arm command

After WS-01/WS-07 clear the editor (or provide a separate RE copy):

```powershell
cd $UEREMCP_ROOT-ws14

# 1) Prepare / score (safe anytime)
python docs/reviews/metrics/run_poc_b_metrics.py `
  --request schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json `
  --out docs/reviews/metrics/artifacts/poc_b_ueremcp_trial.json

# 2) Live call — ONLY with coordination
# Capture monotonic timestamps in the calling agent around call_tool:
#   toolset: UeremcpNiagara
#   tool:    CreateNiagaraEffect
#   arg:     RequestJson = <fixture.request JSON string>
# Then write trial JSON with wall_clock_seconds measured + raw response.
#
# Script gate (still refuses until a future coordinated allow path is added):
# python docs/reviews/metrics/run_poc_b_metrics.py --execute --allow-live
```

Also re-parse the editor log:

```powershell
python -c "from docs.reviews.metrics.parse_editor_log import measure_server_side_interval_file; import json; print(json.dumps(measure_server_side_interval_file(r'$UEREMCP_LEGACY_PROJECT\Saved\Logs\RE.log'), indent=2))"
```

(Prefer importing via `docs/reviews` on `PYTHONPATH` as the unit tests do.)

## Exact primitive baseline trial (WS-07 sequence)

Target asset: `/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline`  
Sequence authority: `docs/proposals/ws-07-poc-b-primitive-baseline-sequence.md`

1. Reset baseline system + generated materials under `/Game/__UeremcpPoc/`.
2. Use only pre-UEREMCP Epic/REAgentTools (+ WS-08 material tools if required).
3. One `ProgrammaticToolset.execute_tool_script` containing the ordered Niagara steps; tally **every** inner `call_tool`.
4. Count every material-chain primitive.
5. Capture client monotonic wall clock; do not use editor log interval as wall clock.
6. Repeat ≥3 clean trials; drop JSON into `docs/reviews/metrics/artifacts/poc_b_baseline_trial_*.json` with fields:
   - `primitive_ops_executed`
   - `mcp_round_trips`
   - `wall_clock_seconds`
   - `completed`
   - per-call raw list

Planned known Niagara minimum excluding OPEN materials + compile polls: **17**
(`docs/reviews/metrics/primitive_baseline.planned_known_minimum`). This is not a measured result.

## Token path

If the trial runs under Cursor MCP: record `unavailable` with the harness reason — do not write `0`.  
If an HTTP MCP client with usage export becomes available: pass usage into `resolve_token_accounting(usage=...)`.

## What would close metrics E7 for POC B

| Cell | Close condition |
|---|---|
| wall_clock_seconds | Measured client monotonic on UEREMCP arm |
| tokens_total | Measured harness usage **or** retain unavailable with reason (honest) |
| primitive_call_equivalent | Measured median/mean from ≥3 baseline trials + wall clock |
| overall claim | Still blocked on B10 / remaining POC-B gates — metrics file alone does not claim POC B |
