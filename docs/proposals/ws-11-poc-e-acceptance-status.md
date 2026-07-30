# WS-11 POC E acceptance status (durability & honesty)

**From:** WS-11  
**Branch:** `ws-11-poc-e-durability` @ worktree `UEREMCP-poc-e`  
**Base tip:** `5235698ad76f2d7fd4e69c4abddf9842151ae4ea`  
**Overall POC E claimed:** **false**  
**Machine bundle:** `tests/integration/_logs/poc_e_criterion_bundle.json`  
(`python tests/poc_evidence.py --poc-e-bundle …` must stay green)

## E1–E7

| # | Status | What was locked | Residual / blocker |
|---|---|---|---|
| E1 | **SKIP / partial** | `UEREMCP.Validation.PocE.Restart.Create` + `Verify` (two-process) + orchestrator `-Scenario E1`; POC B has B8 | Full “all POC A–D survive restart” unmet: C/D not delivered; A has no restart pair; Validation scratch + B8 only |
| E2 | **PASS** | `Rollback.MultiAssetDiscard` | Scoped to Content package-add full Discard (ADR-0005 residuals unchanged) |
| E3 | **PASS (protocol)** | `Idempotency.RepeatedCreate` (shipping 6/6) | Blueprint domain gate authored; Niagara/Material not gated; live Blueprint domain PASS pending cutover |
| E4 | **PASS (protocol)** | `Revision.StaleRejected` (shipping 6/6) | Same as E3 |
| E5 | **SKIP (harness)** | `Honesty.ValidateFalseForbidsValidated` + `UeremcpHonestyContract` | Filter authored + unit-locked; live run blocked (editor occupied / junction on ws01) |
| E6 | **SKIP (harness)** | `Honesty.BrokenRequestFailedValidation` | Same live-run blocker as E5 |
| E7 | **SKIP / partial** | POC B metrics closed in `poc-metrics.md`; A/C/D/E open | WS-14 owns `docs/reviews/**`; handoff in `ws-11-poc-e-metrics-handoff.md` |

## Destructive dry_run default

`UEREMCP.Validation.Honesty.DestructiveDryRunDefault` locks ADR-0010 policy: omitted `dry_run` on delete/replace-on-existing forces `bEffectiveDryRun=true`. Never destroy user content; scratch only under `/Game/__UeremcpTests/`.

## How to re-run

```powershell
cd $UEREMCP_ROOT-poc-e
python tests/run_unit_tests.py
python tests/poc_evidence.py --poc-e-bundle tests/integration/_logs/poc_e_criterion_bundle.json
# After RE junction briefly points at this worktree and Validation rebuilds:
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"
pwsh tests/run_poc_acceptance.ps1 -Scenario E -EvidenceOutput tests/integration/_logs/poc_e_acceptance.json
```

## Non-claims

- No project-complete claim.
- No overall POC E claim while residuals remain.
- Junction left on ws01 unless a coordinated cutover is required for live DLL evidence.
