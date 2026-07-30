# WS-11 proposal: POC A/B8 evidence and metrics handoff

**From:** WS-11  
**To:** WS-06, WS-07, WS-14  
**Status:** Ready for domain filter integration

WS-11 now owns an executable orchestrator at `tests/run_poc_acceptance.ps1` and a
strict parser at `tests/poc_evidence.py`. The harness does not claim A6, POC A, B8,
or overall POC B by itself. Missing domain filters are SKIP; a filter that runs
without evidence is FAIL.

## WS-06 handoff: POC A

Provide this Automation filter:

`UEREMCP.Blueprint.POCA.CompleteRoundTrip`

Emit one compact JSON log line:

`UEREMCP_POC_EVIDENCE={"schema_version":1,"scenario":"poc_a",...}`

For `outcome: "pass"`, the parser requires:

- criteria `A1` through `A11`, each with `status: "pass"`
- A6 fields `expected_nodes_present: true` and
  `expected_connections_present: true`
- numeric `metrics.mcp_round_trips`, `internal_operations`, `tokens_total`,
  `wall_clock_seconds`, and `primitive_call_equivalent`
- `metrics.mcp_round_trips <= 3`

The domain filter remains responsible for the substantive A1-A11 assertions,
including compile/save, response status, hash identity, fidelity disclosure, and
no-recompile idempotency. The harness only rejects structurally incomplete evidence.

Run:

```powershell
pwsh tests/run_poc_acceptance.ps1 -Scenario A `
  -EvidenceOutput tests/integration/_logs/poc_a_evidence.json
```

## WS-07 handoff: B8 restart survival

Provide both filters:

1. `UEREMCP.Niagara.POCB.Restart.Create`
2. `UEREMCP.Niagara.POCB.Restart.Verify`

The harness launches a fresh `UnrealEditor-Cmd` process for each phase. Create emits
`scenario: "poc_b8_create"` with a durable checkpoint ID and non-empty asset list.
Verify emits `scenario: "poc_b8_verify"` with the same checkpoint and assets,
`restart_observed: true`, `reread_after_restart: true`, B8 `status: "pass"`, and
the required numeric metrics. A mismatch is FAIL.

Run:

```powershell
pwsh tests/run_poc_acceptance.ps1 -Scenario B8 `
  -EvidenceOutput tests/integration/_logs/poc_b8_evidence.json
```

The domain filters must clean `/Game/__UeremcpPoc/` only after the verify phase has
captured evidence. A create-only run must not be reported as B8.

## WS-14 handoff: `docs/reviews/poc-metrics.md`

`docs/reviews/**` is WS-14-owned. Please record a row only from a passing, retained
evidence JSON produced by the commands above. Do not scaffold empty numbers or copy
the examples from `tests/benchmark/results_template.csv`.

Minimum numeric columns map directly from the evidence marker:

- `mcp_round_trips`
- `internal_operations`
- `tokens_total`
- `wall_clock_seconds`
- `primitive_call_equivalent`

Also record `run_id`, orch tip, filter name, completion outcome, and log/evidence
path. Completion rate belongs in the review when multiple attempts are aggregated.
