# WS-11 POC E acceptance status (durability & honesty)

**From:** WS-11  
**Branch:** `ws-01-poc-cde-integration` @ worktree `UEREMCP-integration`  
**Tested implementation tip:** `713ad7014caa1472bba4c0f5d6a097a61c866e34`
**Overall POC E claimed:** **true**
**Machine bundle:** `tests/integration/_logs/poc_e_criterion_bundle.json`  
**E1 live:** `tests/integration/_logs/poc_e1_ad_restart_full_20260730.json`
**E harness:** `tests/integration/_logs/poc_e_acceptance_full_20260730.json`
**E7 numbers:** `tests/integration/_logs/poc_e7_metrics_20260730.json`

## E1–E7

| # | Status | What was locked | Residual / blocker |
|---|---|---|---|
| E1 | **PASS (full A–D)** | Two-process Validation.PocE.Restart Create/Verify checkpointed every accepted A–D result plus scratch. `full_ad_results_claimed=true`; all assets survived, and both `DT_POCC_Variations` rows matched their pre-restart snapshots. | — |
| E2 | **PASS** | `Rollback.MultiAssetDiscard` | Scoped to Content package-add full Discard (ADR-0005 residuals unchanged) |
| E3 | **PASS (scoped)** | `Idempotency.RepeatedCreate` + Blueprint domain gate live | Niagara/Material domain pipelines not gated |
| E4 | **PASS (scoped)** | `Revision.StaleRejected` + Blueprint domain gate live | Niagara/Material residuals |
| E5 | **PASS** | `ValidateFalseForbidsValidated` live | — |
| E6 | **PASS** | `BrokenRequestFailedValidation` live | — |
| E7 | **PASS (rows recorded)** | A/B/C/D/E cells in `docs/reviews/poc-metrics.md` + `poc_e7_metrics_20260730.json` | Tokens/`internal_operations`/wall/primitive often `unavailable` with machine-checkable reasons (same honesty as POC B). |

## Assets that survived restart (E1 full)

From verify evidence (`missing_after_restart=[]`):

| POC | Survived assets |
|---|---|
| scratch | `/Game/__UeremcpTests/PocE_Restart/PocERestartCurve` |
| A | `BP_CompleteRoundTripTransport` |
| B | `NS_POCB_Fireball` + six role MIs |
| C | `NS_POCC_IceVariationDirect`, `NS_POCC_IceVariation`, `NS_POCC_WindThirdGeneration` + Core/Trail MIs |
| D | `DT_PocD_Live` |

The C checkpoint also includes
`/Game/__UeremcpPoc/Abilities/DT_POCC_Variations`. Verify loaded the table in
the fresh process and asserted `poc_c_ice_fire_s` and `poc_c_wind_fire_s`
serialized identically to Create. The accepted D5 criterion is the Pattern B
static checklist; its optional multi-client proof is not an E1 durability
asset `[VERIFIED: docs/POC_ACCEPTANCE.md:123-130]`.

## Scoped limitations

E3/E4 remain scoped to their named protocol gates plus the Blueprint domain
gates. Niagara and Material domain-specific idempotency/revision pipelines are
not gated. This limitation is explicit but does not add an unstated
all-domains requirement to E3/E4 `[VERIFIED: docs/POC_ACCEPTANCE.md:143-151]`.

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
- Overall POC E is claimed only under the frozen E1–E7 wording; scoped
  E3/E4 domain coverage remains documented above.
- Junction restored to `UEREMCP-ws01` after this closeout.
