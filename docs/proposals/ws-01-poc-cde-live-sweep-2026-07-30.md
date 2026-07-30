# WS-01 live RE acceptance sweep — POC C/D/E

- **Date:** 2026-07-30
- **RE project:** `$UEREMCP_LEGACY_PROJECT`
- **Integrated code under test:** through `3c27922`
- **POC C source commits:** `ead0cc4` / `b6bb8d4`
- **Integrated POC C equivalents:** `81d819f` / `ddd1890`
- **Integrated POC C live-fix commits:** `2ae2e01` / `a46517e` / `3c27922`
- **Integrated POC D live-fix commits:** `00fd824` / `2ae2e01`
- **Overall result:** partial; POC C, POC D, and POC E are not fully claimed
- **Main ready for another C/D/E fast-forward:** **no**

The RE plugin junction was temporarily moved from `UEREMCP-ws01` to this integration
worktree, the plugin was rebuilt against RE, all commands below used NullRHI, and the
junction was restored to `UEREMCP-ws01` after the sweep. The first build exposed invalid
adjacent `TEXT` macros in three WS-11 POC E tests. The syntax was corrected and the
second RE build succeeded `[VERIFIED-RUNTIME: REEditor Development build completed with
Result: Succeeded on 2026-07-30]`.

## POC C

| # | Result | Live evidence |
|---|---|---|
| C1 | **MET live** | Both live filters completed their requested generation in one reported MCP round trip. |
| C2 | **MET live** | Direct variation preserved all source emitter names and added `Crystalline` plus `IceImpact`; template responses retained inherited/overridden facts. |
| C3 | **MET live** | Core/trail material assets were created and the required Niagara dependency chain completed without rollback. |
| C4 | **MET live** | `NS_POCC_IceVariationDirect` loaded after save; source/target summaries had no edit errors and required user parameters were present. |
| C5 | **FAIL** | No verified networking/damage contract exists for the Niagara source. No preservation claim is made. |
| C6 | **MET live** | The reusable elemental template instantiated the requested ice and wind assets through its canonical plan. |
| C7 | **MET live** | The third-generation filter produced and loaded both ice and wind result assets. |

Filters:

- `UEREMCP.Protocol.PlanExecutor.UsablePartialDependency` — **PASS**
  (`editor_UEREMCP_Protocol_PlanExecutor_UsablePartialDependency_20260730_120033.log`)
- `UEREMCP.Niagara.Create.PocCVariationRuntime` — **PASS**
  (`editor_UEREMCP_Niagara_Create_PocCVariationRuntime_20260730_120054.log`)
- `UEREMCP.Templates.POCC.ThirdGeneration` — **PASS**
  (`editor_UEREMCP_Templates_POCC_ThirdGeneration_20260730_122445.log`)

Live fixes:

1. The C++ and Python envelope parsers now accept the ADR-0010/schema-defined
   `options.allow_destructive` field.
2. `execute_plan` now permits an honest `partially_completed` dependency to continue
   only when its response contains a usable `result.primary_asset`; aggregate status
   remains `partially_completed`, so `validate:false` never becomes `*_validated`.
3. Template materialization binds a named terminal operation to the requested target
   asset identity instead of creating the generic template name in the target folder.

POC C remains **partial overall** because C5 is still an intentional honest failure.

## POC D

| # | Result | Live evidence |
|---|---|---|
| D1 | **MET live** | `LiveUpsertViaPlan` passed a real `execute_plan` row upsert. |
| D2 | **MET live** | `UEREMCP.Gameplay.PocD.DependsOnAndRef` passed dependency-order assertions. |
| D3 | **MET live** | The same test passed nested `$ref` substitution assertions. |
| D4 | **MET live** | Passing Gameplay automation asserted the requested RE spell identity fields. |
| D5 | **MET static only** | RE Pattern B remains a static checklist; no multi-client proof is claimed. |
| D6 | **MET live** | `ElementColor` imported as an `FLinearColor` object; save/re-read validation passed. |
| D7 | **MET live** | `RollbackOnFailure` passed with aggregate `rolled_back` and no partial result claim. |
| D8 | **MET live** | Passing plan tests asserted one consolidated response with per-op results. |

Filters:

- `UEREMCP.Gameplay` — **10/10 PASS**
  (`editor_UEREMCP_Gameplay_20260730_120758.log`).
- `UEREMCP.Gameplay.PocD` — **3/3 PASS**
  (`editor_UEREMCP_Gameplay_PocD_20260730_120648.log`).
- `UEREMCP.Protocol.PlanExecutor` — **10/10 PASS**, including the zero-success
  regression (`editor_UEREMCP_Protocol_PlanExecutor_20260730_120828.log`).

The four-number `ElementColor` array was replaced by the object representation consumed
by RE's `FLinearColor` UStruct field. The live run also exposed and fixed a second
request-composition issue: nested plan request IDs contain `:`, so the DataTable mutator
now sanitizes FileSandbox names before `FGlobalSandbox::Enter`
`[VERIFIED-RUNTIME: LiveUpsertViaPlan passed save/re-read on 2026-07-30]`.

The executor now guards the zero-success aggregate path and reports
`failed_validation` when no operation completed and no rollback occurred. Overall POC D
is still **not** claimed: D5 remains static-only until a multi-client runtime proof lands.

## POC E

| # | Result | Live evidence |
|---|---|---|
| E1 | **NOT MET overall** | The two-process validation scratch create/verify pair passed with `restart_observed=true`, but all POC A-D restart survival is not proven; C and D failed this sweep. |
| E2 | **MET live** | `Rollback.MultiAssetDiscard` passed. |
| E3 | **MET live, scoped** | `Idempotency.RepeatedCreate` and Blueprint domain repeated-replace passed; Niagara/Material and cross-restart persistence remain residuals. |
| E4 | **MET live, scoped** | `Revision.StaleRejected` and the Blueprint domain stale-revision gate passed; Niagara/Material remain residuals. |
| E5 | **MET live** | `ValidateFalseForbidsValidated` passed with machine evidence. |
| E6 | **MET live** | `BrokenRequestFailedValidation` passed with `failed_validation` machine evidence. |
| E7 | **NOT MET** | Complete measured metrics for POC A/C/D/E are still absent. |

Passing commands:

- `UEREMCP.Validation.Honesty` — 3/3 pass
- `UEREMCP.Validation.Domain` — 2/2 pass
- `UEREMCP.Validation.PocE` — 2/2 pass
- `UEREMCP.Validation.Rollback.MultiAssetDiscard` — pass
- `UEREMCP.Validation.Idempotency.RepeatedCreate` — pass
- `UEREMCP.Validation.Revision.StaleRejected` — pass
- `run_poc_acceptance.ps1 -Scenario E` — harness result `pass` for its intentionally
  limited E5/E6/validation-scratch E1 scope

The orchestrator output is
`tests/integration/_logs/poc_e_acceptance_live_20260730.json`. Its `pass` does **not**
mean overall POC E passes; the script explicitly covers only E5, E6, and a validation
scratch restart pair.

## Remaining finish-the-plugin residuals

1. Add a real networking/damage source contract before reconsidering C5.
2. Add the D5 multi-client networking proof; static Pattern B checks remain the only
   unresolved POC D criterion.
3. Extend E1 restart proof to all successful POC A-D results and close E7 metrics.
