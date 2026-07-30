# WS-01 editor automation filter results

- **Current orchestration tip:** `fcff5cc` (WS-11 fireball material proof scaffold; editor proof SKIP)
- **Latest Blueprint re-run tip:** `35b4cab`
- **Latest Animation re-run tip:** `5ea9277`
- **Latest Niagara re-run tip:** `2384112`
- **Latest Material re-run tip:** `7535e6c`
- **Latest Templates re-run tip:** `f15ea96`
- **Latest live VisualTest MCP T1a tip:** `7535e6c` lineage (editor PID 38668)
- **Prior mixed re-run tip:** `c234606`
- **Date:** 2026-07-30
- **Status:** Wave 2 listed editor filters are green on current-tip or unchanged-source proofs. Material is **PASS 11/11** on `7535e6c`, including ValidateFalse disk persistence; Templates is **PASS 4/4** on `f15ea96`; Niagara Create/Inspect/B7 are green. Optional live VisualTest MCP `BENCHMARK_PROTOCOL` T1a freshness is **PASS** after editor restart. No A6 / overall POC-B completion claim.
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
| `UeremcpMaterial.Toolset` | **PASS, 11/11** | `7535e6c` | ValidateFalse and MI/master disk persistence are green; live VisualTest MCP T1a freshness also PASS. |
| `UEREMCP.Animation` | **PASS, 10/10** | `5ea9277` | Animation sources unchanged since proof. |
| `UEREMCP.Niagara.Create` | **PASS, 10/10** | `2384112` | Current-tip freshness re-run closed. |
| `UEREMCP.Niagara.Inspect` | **PASS, 4/4** | `2384112` | Current-tip freshness re-run closed. |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) | **PASS, 1/1** | `825e4f4` | Current-lineage proof. B7 only; not overall POC-B. |
| `UeremcpTemplates.Toolset` | **PASS, 4/4** | `f15ea96` | Plugin-local template seeds resolved the Search/Promote failures. |

Residuals: optional live VisualTest MCP T1a freshness is **PASS**. B2/B4 wiring ready after `51583af`/`150f61a`. WS-11 fireball inline-material filter scaffold landed (`fcff5cc` / `11efc23`) but editor proof was **SKIP** — Niagara rejects `/Game/__UeremcpPoc/`; Material paths remain under test roots (`docs/proposals/ws-11-pocb-poc-root-blocker.md`). **Do not claim B1/B2/B4.** Allowlist owners: WS-07 + WS-08. **A6 and overall POC-B remain unclaimed** — `docs/proposals/ws-01-a6-pocb-residual-plan.md`.

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
| WS-07 | B7 scaffold PASS. B2/B4 wiring on `2814283`. **Own POC-root allowlist** so fireball filter can leave SKIP (`ws-11-pocb-poc-root-blocker.md`). B1/B8 still open. |
| WS-08 | Material reuse slice on `b1f9479`. **Own Material path allowlist** for `/Game/__UeremcpPoc/` acceptance assets (same blocker). |
| WS-10 | Animation Toolset PASS 10/10 on `5ea9277`; no further Animation filter work from this triage. |
| WS-11 | Fireball filter scaffold on `fcff5cc` — proof **SKIP**, not B1/B2/B4. Re-run after WS-07/WS-08 allowlist. |
| WS-15 | Templates PASS 4/4 on `f15ea96`; no remaining Templates filter failure in this record. |

Wave 2 listed editor filters are green on recorded tips. B2/B4 wiring is ready; fireball editor proof is **SKIP** pending WS-07/WS-08 `/Game/__UeremcpPoc/` allowlist. Remaining acceptance gates: A6 and overall POC-B — see `docs/proposals/ws-01-a6-pocb-residual-plan.md`. No junction retarget.
