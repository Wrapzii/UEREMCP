# WS-11 POC E acceptance status (durability & honesty)

**From:** WS-11  
**Branch:** `ws-01-poc-cde-integration` @ worktree `UEREMCP-integration`  
**Base tip at closeout:** `785300383b3ab5fac2ad06199c78f3dbaa411725` (+ pending WS-11/WS-14/WS-01 commits)  
**Overall POC E claimed:** **false**  
**Machine bundle:** `tests/integration/_logs/poc_e_criterion_bundle.json`  
**E1 live:** `tests/integration/_logs/poc_e1_ad_restart_clean_20260730.json`  
**E harness:** `tests/integration/_logs/poc_e_acceptance_20260730_e7close.json`  
**E7 numbers:** `tests/integration/_logs/poc_e7_metrics_20260730.json`

## E1–E7

| # | Status | What was locked | Residual / blocker |
|---|---|---|---|
| E1 | **PASS (partial scope)** | Two-process Validation.PocE.Restart Create/Verify after seeding B/C/D; checkpointed A+B+C+D+scratch all survived restart (`survived_by_poc` all_survived=true). `full_ad_results_claimed=false`. | Overall E1 (“all POC A–D results”) unmet: **C5 FAIL** (no networking/damage assets to survive); **D5** static-only Pattern B (DT survival ≠ multi-client). TransportFixture.Setup recreate crashes if BP already loaded — seed skipped; A CRT asset still checkpointed. |
| E2 | **PASS** | `Rollback.MultiAssetDiscard` | Scoped to Content package-add full Discard (ADR-0005 residuals unchanged) |
| E3 | **PASS (scoped)** | `Idempotency.RepeatedCreate` + Blueprint domain gate live | Niagara/Material domain pipelines not gated |
| E4 | **PASS (scoped)** | `Revision.StaleRejected` + Blueprint domain gate live | Niagara/Material residuals |
| E5 | **PASS** | `ValidateFalseForbidsValidated` live | — |
| E6 | **PASS** | `BrokenRequestFailedValidation` live | — |
| E7 | **PASS (rows recorded)** | A/B/C/D/E cells in `docs/reviews/poc-metrics.md` + `poc_e7_metrics_20260730.json` | Tokens/`internal_operations`/wall/primitive often `unavailable` with machine-checkable reasons (same honesty as POC B). **Not** an overall POC E claim. |

## Assets that survived restart (E1 partial)

From verify evidence (`missing_after_restart=[]`):

| POC | Survived assets |
|---|---|
| scratch | `/Game/__UeremcpTests/PocE_Restart/PocERestartCurve` |
| A | `BP_CompleteRoundTripTransport` |
| B | `NS_POCB_Fireball` + six role MIs |
| C | `NS_POCC_IceVariationDirect`, `NS_POCC_IceVariation`, `NS_POCC_WindThirdGeneration` + Core/Trail MIs |
| D | `DT_PocD_Live` |

**Not in checkpoint (criteria still fail):** C5 networking/damage contract (none exists); D5 multi-client proof (not an asset).

## Destructive dry_run default

`UEREMCP.Validation.Honesty.DestructiveDryRunDefault` locks ADR-0010 policy: omitted `dry_run` on delete/replace-on-existing forces `bEffectiveDryRun=true`. Never destroy user content; scratch only under `/Game/__UeremcpTests/`.

## How to re-run

```powershell
cd $UEREMCP_ROOT-integration
python tests/unit/test_poc_evidence.py
python tests/poc_evidence.py --poc-e-bundle tests/integration/_logs/poc_e_criterion_bundle.json
# Junction briefly at this worktree; rebuild Validation; then:
pwsh tests/run_poc_acceptance.ps1 -Scenario E1 -EvidenceOutput tests/integration/_logs/poc_e1_ad_restart.json
pwsh tests/run_poc_acceptance.ps1 -Scenario E -SkipDomainSeed -EvidenceOutput tests/integration/_logs/poc_e_acceptance.json
# Restore RE Plugins/UEREMCP junction to UEREMCP-ws01 afterwards.
```

## Non-claims

- No project-complete claim.
- **No overall POC E claim** — E1 full A–D unmet while C5/D5 remain.
- Junction restored to `UEREMCP-ws01` after this closeout.
