# WS-01 live RE acceptance sweep — POC C/D/E

- **Date:** 2026-07-30
- **RE project:** `$UEREMCP_LEGACY_PROJECT`
- **Integrated code under test:** `ddd18909f68c4fb840b0834de41ff0852e94ce22`
- **POC C source commits:** `ead0cc4` / `b6bb8d4`
- **Integrated POC C equivalents:** `81d819f` / `ddd1890`
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
| C1 | **NOT MET** | The canonical MCP call was one round trip, but returned `rolled_back`; the direct runtime envelope was rejected. |
| C2 | **NOT MET live** | The response reported `inherited:*` / `overridden:*`, but no variation asset was produced. |
| C3 | **NOT MET live** | Ice material creation returned `partially_completed`; the required plan then rolled back. |
| C4 | **NOT MET live** | `NS_POCC_IceVariationDirect` did not exist after the runtime filter. |
| C5 | **FAIL** | No verified networking/damage contract exists for the Niagara source. No preservation claim is made. |
| C6 | **MET offline only; NOT MET live** | The conforming reusable template is present, but its live instantiation rolled back. |
| C7 | **NOT MET live** | The third-generation filter failed; neither ice nor wind result asset existed. |

Filters:

- `UEREMCP.Niagara.Create.PocCVariationRuntime` — **FAIL**
  (`editor_UEREMCP_Niagara_Create_PocCVariationRuntime_20260730_114317.log`)
- `UEREMCP.Templates.POCC.ThirdGeneration` — **FAIL**
  (`editor_UEREMCP_Templates_POCC_ThirdGeneration_20260730_114410.log`)

Exact live blockers:

1. The direct POC C test sends `options.allow_destructive`; the live envelope parser
   rejects that unknown field before mutation, so the test subsequently cannot load
   `NS_POCC_IceVariationDirect`.
2. The canonical template call returns `rolled_back`. Its required `core_material`
   operation returns `partially_completed` because it executes with
   `options.validate=false`; `trail_material` and `projectile_fx` are then skipped.
3. C5 remains an intentional honest failure, independent of the runtime defects.

## POC D

| # | Result | Live evidence |
|---|---|---|
| D1 | **NOT MET live** | `LiveUpsertViaPlan` failed before a verified row upsert. |
| D2 | **MET live** | `UEREMCP.Gameplay.PocD.DependsOnAndRef` passed dependency-order assertions. |
| D3 | **MET live** | The same test passed nested `$ref` substitution assertions. |
| D4 | **MET in passing contract test** | The passing test asserted the requested RE spell identity fields. |
| D5 | **MET static only** | RE Pattern B remains a static checklist; no multi-client proof is claimed. |
| D6 | **NOT MET live** | DataTable import rejected `ElementColor`; save/re-read was not completed. |
| D7 | **NOT MET live** | `RollbackOnFailure` failed while seeding the row, before the intended rollback proof. |
| D8 | **MET in passing contract test** | The passing plan test asserted one consolidated operation response. |

Filters:

- `UEREMCP.Gameplay` — **CRASH** at
  `FUeremcpPlanExecutor::ExecuteRequest`, `UeremcpPlanExecutor.cpp:719`;
  `SuccessfulResponses.Last()` was evaluated when the array was empty
  (`editor_UEREMCP_Gameplay_20260730_114443.log`).
- `UEREMCP.Gameplay.PocD` — **1 PASS / 2 FAIL**:
  `DependsOnAndRef` passed; `LiveUpsertViaPlan` and `RollbackOnFailure` failed
  (`editor_UEREMCP_Gameplay_PocD_20260730_114851.log`).

The live row blocker is a contract mismatch: `FUeremcpSpellPlanner` emits
`ElementColor` as a four-number JSON array, while RE's DataTable UStruct import rejects
that representation (`array size ... has 4 elements, but needs 1`). POC D therefore
remains code-partial and is not upgraded to live-MET.

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

1. Remove the unsupported `allow_destructive` field from the POC C direct envelope (or
   add it to the frozen request contract through WS-01, if truly required).
2. Ensure template construction operations inherit `validate=true` and do not treat an
   honest but incomplete material response as sufficient for a required dependency.
3. Add a real networking/damage source contract before reconsidering C5.
4. Serialize `ElementColor` in the RE DataTable representation accepted by
   `FJsonObjectConverter`, then rerun POC D live upsert and rollback
   `[VERIFIED-RUNTIME: the 2026-07-30 PocD filter rejected the four-number array during
   UStruct import]`.
5. Guard `SuccessfulResponses.Last()` when a non-rollback plan has zero successful
   operations.
6. Extend E1 restart proof to all successful POC A-D results and close E7 metrics.
