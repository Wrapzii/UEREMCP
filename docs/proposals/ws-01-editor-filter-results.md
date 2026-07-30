# WS-01 editor automation filter results

- **Current orchestration tip:** `43b8a5a` (WS-07 inline Niagara material co-location fix)
- **Latest Blueprint acceptance re-run tip:** `d691316` (**FAIL**, PocA6Reread)
- **Latest Animation re-run tip:** `5ea9277`
- **Latest Niagara re-run tip:** `2384112`
- **Latest Material re-run tip:** `d691316` (**PASS 14/14**)
- **Latest Templates re-run tip:** `f15ea96`
- **Latest live VisualTest MCP T1a tip:** `7535e6c` lineage (editor PID 38668)
- **Prior mixed re-run tip:** `c234606`
- **Date:** 2026-07-30
- **Status:** Material is **PASS 14/14** on `d691316`; prior Wave 2 Niagara Create/Inspect/B7 and Templates proofs remain green. Acceptance follow-ups are **not green**: Fireball POC FAIL and Blueprint PocA6Reread FAIL on `d691316`. No A6 / POC A / B1 / B2 / B4 / overall POC-B completion claim.
- **Junction:** Not changed.

## Invocation

Each suite used the established runner:

```powershell
pwsh -NoProfile -File "tests/run_editor_tests.ps1" -KeepUeremcp -NoProbe -Filter "<filter>"
```

The runner launched `UnrealEditor-Cmd.exe` with `-unattended -nop4 -nosplash -NullRHI -nosound` and `Automation RunTests <filter>; Quit`.

## Wave 2 editor evidence summary

| Filter / gate | Recorded result | Proof tip | Freshness / residual |
|---|---:|---|---|
| `UeremcpBlueprint.Toolset` | **PASS, 4/4** | `35b4cab` | Blueprint sources unchanged since proof. |
| `UeremcpMaterial.Toolset` | **PASS, 14/14** | `d691316` | Current WS-11 runtime proof; log below. |
| `UEREMCP.Animation` | **PASS, 10/10** | `5ea9277` | Animation sources unchanged since proof. |
| `UEREMCP.Niagara.Create` | **PASS, 10/10** | `2384112` | Current-tip freshness re-run closed. |
| `UEREMCP.Niagara.Inspect` | **PASS, 4/4** | `2384112` | Current-tip freshness re-run closed. |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **PASS, 1/1** | `825e4f4` | Current-lineage proof. B7 only; not overall POC-B. |
| `UeremcpTemplates.Toolset` | **PASS, 4/4** | `f15ea96` | Plugin-local template seeds resolved the Search/Promote failures. |

Residuals: A6 runtime **FAIL** on `d691316`: missing BeginPlay→Branch link; A8/A11 no-op failed. Fireball runtime **FAIL** on `d691316`: MIs under test root, B4 false, B2 harness manifest-path issue. Parsing fix is `674c439`; WS-07 MI co-location fix is now `43b8a5a` (`60cb3a4`) — **WS-11 re-run required; B4/POC-B not claimed**. Material **PASS 14/14**. No A6/POC-A/B1/B2/B4/overall POC-B claim.

## Acceptance runtime follow-up on tip `d691316` (WS-11)

| Proof | Result | Evidence / owner |
|---|---:|---|
| Fireball POC | **FAIL** | `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_050816.log`: Niagara system under POC root; MIs under test root; B4 false; B2 harness manifest-path issue. WS-07/WS-08 own MI co-location. |
| Blueprint `PocA6Reread` | **FAIL** | `editor_UeremcpBlueprint_Toolset_PocA6Reread_20260730_050942.log`: `failed_validation`; BeginPlay→Branch link missing; A8/A11 no-op failed. WS-06 owns fix. |
| Material Toolset | **PASS, 14/14** | `editor_UeremcpMaterial_Toolset_20260730_051149.log`. |

WS-11 parsing fix is integrated as `674c439`. WS-07 co-location fix is integrated as `43b8a5a`; neither proves B2/B4 until WS-11 re-runs.

## Templates editor result and handoff

- **Owner:** WS-15 owns `Plugins/UEREMCP/Source/UeremcpTemplates/**` (`docs/WORK_ALLOCATION.md`, WS-15 row). WS-11 owns the shared harness and runs domain filters.
- **Filter landed:** WS-15 commit `1480e7d` is integrated as `b709b65`, registering four `UeremcpTemplates.Toolset` editor tests.
- **WS-11 result on `b709b65`:** **FAIL 2/4**. `Register` and `Instantiate.Validation` passed. `Promote.Preview` failed because the operation returned `failed_validation` where the test expected `partially_completed`. `Search` failed because the seeded projectile template was missing from results.
- **WS-15 revision:** plugin-local template seeds commit `2817832` is integrated as `f15ea96`.
- **WS-11 result on `f15ea96`:** **PASS 4/4**. The Templates Search/Promote residual is closed.

WS-01 did not edit WS-15-owned implementation paths.

## Blueprint triage re-proof on tip `35b4cab`

`35b4cab` contains integrated fixes `26ce2d6` (hash alignment; original `fc51ad2`) and `2d2f7ef` (SubmitGraph DSL write-intent; original `443c298`). The `UeremcpBlueprint.Toolset` filter was rebuilt and returned PASS 4/4. No Blueprint source changed between `35b4cab` and the current lineage.

| Item | Result | Exact editor evidence |
|---|---:|---|
| SubmitGraphValidation | **PASS** | `UeremcpBlueprint.Toolset.SubmitGraphValidation`, `tests/integration/_logs/editor_UeremcpBlueprint_Toolset_20260730_022304.log`: `Test Completed. Result={Success}` at line 3019. |
| Revision-hash mismatch / hash alignment | **PASS** | `UeremcpBlueprint.Toolset.ReadGraphRoundTrip`, same log: `Test Completed. Result={Success}` at line 2999. The test asserts graph `content_hash == revision` and summary revision equals complete revision. |

No additional rerun was needed because the exact proof tip remains an ancestor of the current lineage and the Blueprint module/test sources are unchanged since that proof.

## Update on tip `2384112` (WS-11 Niagara Create/Inspect freshness)

| Filter | Result | Owner | Exact evidence |
|---|---:|---|---|
| `UEREMCP.Niagara.Create` | **PASS, 10/10** | WS-07 | `tests/integration/_logs/editor_UEREMCP_Niagara_Create_20260730_033148.log`: Found 10 tests; all Success; `TEST COMPLETE. EXIT CODE: 0` at line 3059. |
| `UEREMCP.Niagara.Inspect` | **PASS, 4/4** | WS-07 | `tests/integration/_logs/editor_UEREMCP_Niagara_Inspect_20260730_033224.log`: Found 4 tests; all Success; `TEST COMPLETE. EXIT CODE: 0` at line 3012. |

Create/Inspect freshness residual on the current tip is closed.

## Update on tip `825e4f4` (WS-11 Niagara B7 re-run)

| Filter | Result | Owner | Exact evidence |
|---|---:|---|---|
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **PASS, 1/1** | WS-07 | `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_SixEmitterGateScaffold_20260730_032748.log`: test Success at line 3005; `UEREMCP_POC_B_GATE_OUTCOME=PASS proof=editor_create_reread_honesty` at line 3007; editor exit 0 at line 3013. |

This proves B7 only. It does not prove overall POC-B or A6.

## Update on tip `81d11dc` (WS-11 Niagara B7 re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **FAIL, assertion_failure** | WS-07 | The dependency-survey replacement was insufficient: the same `bOverrideMaterials` EditCondition `LogError` remains the sole failure. Not a B7 / POC-B completion claim. |

Standing by for the next WS-07 fix.

## Update on tip `d57f09d` (WS-11 Niagara B7 re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **FAIL, assertion_failure** | WS-07 | Only remaining failure is the `bOverrideMaterials` EditCondition `LogError`. Prior cleanup ensure, inspect crash, and link failure are resolved. Not a B7 / POC-B completion claim. |

Standing by for the final WS-07 EditCondition fix.

## Update on tip `eff241c` (WS-11 Niagara B7 re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **FAIL, assertion_failure** | WS-07 | Inspect null AV is resolved. `bOverrideMaterials` still emits `LogEditCondition`, followed by a `ForceDeleteObjects` cleanup ensure. Not a B7 / POC-B completion claim. |

Standing by for a deeper WS-07 override/cleanup fix.

## Update on tip `8c7cd8d` (WS-11 Niagara B7 re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **CRASH** | WS-07 | Null access violation in `FUeremcpNiagaraInspect::Run` at line 412 during round-trip after create. Not a B7 / POC-B completion claim. |

Standing by for the WS-07 crash fix.

## Update on tip `b5b07e1` (WS-11 Niagara re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Niagara.Inspect` | **PASS, 4/4** | WS-07 | Deeper `WaitForCompilationComplete` AwaitCompile fix resolved the compile stall. |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **FAIL, assertion_failure** | WS-07 | `bOverrideMaterials` edit-condition `LogError`, followed by a cleanup ensure on `NS_POCB_FireballProbe`. Not a B7 / POC-B completion claim. |

Standing by for the WS-07 B7 fix on the `e4ea58d` lineage.

## Update on tip `5ea9277` (WS-10 Animation re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Animation` | **PASS, 10/10** | WS-10 | Skeleton-safe transient fixture and notify-state duration tolerance fixes landed; offline suite also reported 17/17. |

## Update on tip `e7f9ae5` (WS-11 Niagara re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UEREMCP.Niagara.Inspect` | **INCOMPLETE** | WS-07 | Hang persists after FTSTicker + `Poll(true)` + drain fix; no completion marker and no timeout-honest result. Not a PASS. |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **INCOMPLETE** | WS-07 | Hang persists after the same AwaitCompile fix; no completion marker and no timeout-honest result. Not a B7 / POC-B claim. |

Standing by for a deeper WS-07 fix before another Inspect/B7 re-run.

## Update on tip `7535e6c` (WS-11 Material re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | **PASS, 11/11** | WS-08 | `CreateVfxMaterial.ValidateFalse` and MI/master disk-persistence checks passed. The T1a-related editor-filter residual is closed. |

### Live VisualTest MCP T1a freshness (PASS)

After editor restart, optional live MCP freshness against the `7535e6c` Material lineage succeeded:

- VisualTest `UnrealEditor` PID `38668`; crash monitor re-armed PID `14548`
- `list_toolsets` OK on `http://127.0.0.1:8001/mcp`
- T1a `create_vfx_material` with `validate:false` returned `partially_completed` as contract-expected (45 internal ops / 1 MCP round trip)
- Disk verified under `/Game/__UeremcpTests/`: MI ~11637 bytes, master ~16225 bytes

This is live MCP freshness on VisualTest, not a replacement for the Material editor-filter PASS 11/11.

## Update on tip `942e8bc` (WS-11 Material re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | **FAIL, 10/11** | WS-08 | `CreateVfxMaterial.ValidateFalse` remains the sole failure: the MI is still absent on disk after `5c5cde8`. |

Standing by for the next WS-08 fix and fresh editor re-run.

## Update on tip `a29308e` (WS-11 Material re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | **FAIL, 10/11** | WS-08 | `CreateVfxMaterial.ValidateFalse` remains the sole failure. Master disk-save now succeeds; only the MI remains absent on disk. |

Standing by for the WS-08 MI-only fix and fresh editor re-run.

## Update on tip `75a72ae` (WS-11 Material re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | **FAIL, 10/11** | WS-08 | `CreateVfxMaterial.ValidateFalse` remains the sole failure: both MI and master are absent on disk despite `ddb1fc8`. |

Standing by for a deeper WS-08 disk-save fix and fresh editor re-run.

## Update on tip `0f5b8bd` (WS-11 Material re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | **FAIL, 10/11** | WS-08 | Sole failure: `CreateVfxMaterial.ValidateFalse`. The MI is not present on disk and an unexpected master dependency remains; benchmark T1a disk-save is not green. |

Standing by for the WS-08 revision and a fresh editor re-run.

## Update on tip `c881742` (WS-11 Material re-run)

| Filter | Result | Owner | Notes |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | **PASS, 10/10** | WS-08 | LoadAsset gate (`af02b15`) + C2440 `TryLoadTexture` fix (`4944eeb` → orch `c881742`). |
| `UEREMCP.Niagara.Inspect` | still INCOMPLETE | WS-07 | Post-compile stall remains; not a PASS. |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | still INCOMPLETE | WS-07 | Post-compile stall remains; not a B7 / POC-B claim. |

Standing by for WS-07 AwaitCompile stall revise before Inspect/B7 re-run.

## Re-run on tip `c234606` (WS-11; historical)

### PASS

| Filter | Result | Owner |
|---|---:|---|
| `UeremcpBlueprint.Toolset` | PASS, 4/4 | WS-06 |
| `UEREMCP.Niagara.Create` | PASS, 10/10 | WS-07 |

### INCOMPLETE (stalled; no completion marker)

| Filter | Progress | Owner | Notes |
|---|---|---|---|
| `UEREMCP.Niagara.Inspect` | 1/4 then stalled | WS-07 | Stalled after `NS_WS07_Probe` compile. Not a PASS. |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | stalled | WS-07 | Stalled after creating/compiling `NS_POCB_FireballProbe`. Not a B7 or POC-B completion claim. |

### FAIL (as of `c234606`; Material and Animation superseded by later PASS results)

| Filter | Result | Owner | Evidence |
|---|---:|---|---|
| `UeremcpMaterial.Toolset` | 5/10 (superseded: now PASS on `c881742`) | WS-08 | VFX tests returned `failed_validation` (missing generated master/MI) where expected status was `partially_completed`. |
| `UEREMCP.Animation` | 8/10 (superseded: now PASS on `5ea9277`) | WS-10 | `InspectMontage.NotifyOrdering`: invalid track name did not degrade to empty. `InspectMontage.StructuredState`: transient asset had no Movie Scene; notify duration assert failed. |

### Not claimed by this re-run

- No A6 claim from Blueprint MutatingDispatch adapter gate.
- No POC-B / B7 completion claim from the stalled Niagara B7 gate.
- Templates automation filter registration remains out of scope for this note unless separately reported.

## Prior baseline on tip `fcdf2e5` (superseded for triage)

Recorded for history. Do not treat as current truth after `c234606`.

| Filter | Result |
|---|---:|
| `UEREMCP.Niagara.Inspect` | FAIL, 3/4 (`NS_WS07_Probe` rejected) |
| `UEREMCP.Niagara.Create` | FAIL, 8/10 |
| `UeremcpMaterial.Toolset` | FAIL, 3/10 |
| `UeremcpBlueprint.Toolset` | FAIL, 2/4 |
| `UEREMCP.Animation` | FAIL, 5/10 |
| `UEREMCP.Niagara.PlanHandlers` | PASS, 3/3 |
| `UEREMCP.Material.PlanHandlers` | PASS, 3/3 |
| `UEREMCP.Transport.Handoff` | PASS, 1/1 |
| `UEREMCP.Protocol.PlanExecutor` | PASS, 7/7 |
| Templates | SKIP / unavailable |

Evidence logs from that baseline remain under `tests/integration/_logs/editor_*_20260730_021*.log`.

## Ownership handoff

| Owner | Next work |
|---|---|
| WS-06 | POC A / A6: deliver programmatic modify→re-read proof; Wave 2 Blueprint 4/4 is not A6. See `docs/proposals/ws-01-a6-pocb-residual-plan.md`. |
| WS-06 | Fix missing BeginPlay→Branch link and A8/A11 no-op after PocA6Reread **FAIL** on `d691316`. |
| WS-07 | MI co-location fix landed as `43b8a5a`; B4 remains unproven pending WS-11 re-run. |
| WS-08 | Material **PASS 14/14**; fireball still needs MI co-location under POC root with WS-07. |
| WS-10 | Animation Toolset PASS 10/10 on `5ea9277`; no further Animation filter work from this triage. |
| WS-11 | Re-run fireball after `43b8a5a`; re-run A6 after WS-06 fix. Keep A6/POC-A/B2/B4/POC-B unclaimed until PASS. |
| WS-15 | Templates PASS 4/4 on `f15ea96`; no remaining Templates filter failure in this record. |

Material is green at 14/14. A6 still fails; Fireball has a co-location fix landed but no re-proof. No A6 / POC-A / B1 / B2 / B4 / overall POC-B claims. No junction retarget.
