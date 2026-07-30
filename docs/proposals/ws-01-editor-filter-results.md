# WS-01 editor automation filter results

- **Current orchestration tip:** `0049153` (B10 harness now ticks/dark-backdrop; production fireball still FAIL)
- **Latest Blueprint acceptance re-run tip:** `3756244` (**overall POC A**, CompleteRoundTrip A1–A11)
- **Latest Animation re-run tip:** `5ea9277`
- **Latest Niagara re-run tip:** `2384112`
- **Latest Material re-run tip:** `d691316` (**PASS 14/14**)
- **Latest Templates re-run tip:** `f15ea96`
- **Latest live VisualTest MCP T1a tip:** `7535e6c` lineage (editor PID 38668)
- **Prior mixed re-run tip:** `c234606`
- **Date:** 2026-07-30
- **Status:** **Overall POC A CLAIMED** via CRT on `3756244` (A1–A11 PASS). B10 harness methodology defect is closed on `0049153` (canary PASS). Production `/Game/__UeremcpPoc/NS_POCB_Fireball` still FAIL: particles spawn (`185`) but warm fire signature is absent (`0`). **No overall POC-B claim.**
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

Residuals: **overall POC A claimed** on CRT `3756244`. Post-UV editor fireball and B8 restart PASS; post-`d07f8f1` MCP B1/B6 PASS. B10 harness can observe particles (canary PASS on `0049153`); production fireball still has no warm signature. Complete metrics/baseline remain open. No overall POC-B claim.

## B10 harness fixed — canary PASS, production still FAIL (WS-11, tip `0049153`)

WS-11 closed the methodology defect identified in the first B10 run:

- Latent ~1.5s editor-frame warm-up so Niagara can simulate
  [VERIFIED: `AutomationTest.h` latent helpers; editor frame pump].
- Settled dark backdrop + forced Niagara show flags.
- Particle-count evidence in the outcome markers.
- Canary path against a known-good flame system.

| Path | Outcome | changed | warm | particles |
|---|---|---:|---:|---:|
| Production `/Game/__UeremcpPoc/NS_POCB_Fireball` | **FAIL** `visible_fire_signature_not_observed` | 5405 | **0** | **185** |
| Known-good canary | **PASS** `viewport_pixel_delta_with_fire_signature` | 5405 | **22** | 43 |

Artifacts:
- `tests/integration/_artifacts/poc_b10_fireball.png` (30,370 bytes)
- `tests/integration/_artifacts/poc_b10_canary.png` (30,835 bytes)

### Consequence for the prior "zero particles" diagnosis

The first B10 run's "system completed in 62 ms / no particles" observation was partly a
harness artifact (no world tick). With ticks, the production system reports **185 live
particles** but still **0 warm pixels**. Remaining defect ownership:

- **WS-07:** generated emitters/renderers/colors produce no warm fire signature (and any
  residual system-state fast-path / scaffold issues in create).
- **WS-08:** only if WS-07 proves materials are the invisible layer (opacity/emissive/blend).
- **WS-11:** harness methodology closed; do not soften warm thresholds to manufacture PASS.

## B10 visible render — first actual execution (WS-11, tip `2f40f24`): FAIL

The B10 filter had never executed before this run. `01b257e` added it but `UeremcpValidation`
would not compile: `NiagaraPocBVisibleRender.spec.cpp` used `GCurrentLevelEditingViewportClient`,
which `Editor.h` only forward-declares
(`extern UNREALED_API class FLevelEditorViewportClient*`
[VERIFIED: `Engine/Source/Editor/UnrealEd/Public/Editor.h:948`]), without including the header
that defines the class
[VERIFIED: `Engine/Source/Editor/UnrealEd/Public/LevelEditorViewport.h:166`]. The incomplete type
made the derived-to-base pointer conversion illegal (`C2440` at line 131) and `GetViewport`
unresolvable (`C2039` at line 132). Adding `#include "LevelEditorViewport.h"` fixed it; `UnrealEd`
was already a private dependency, so no `Build.cs` change was required.

With a real (non-`NullRHI`) viewport, the gate ran and reported:

```
UEREMCP_POC_B10_EVIDENCE={"status":"fail","width":1530,"height":605,
  "changed_pixels":5457,"warm_changed_pixels":0,"programmatic_pixel_validation":true}
UEREMCP_POC_B10_OUTCOME=FAIL reason=visible_fire_signature_not_observed
```

Artifact: `tests/integration/_artifacts/poc_b10_fireball.png` (27,115 bytes, 1530×605). The image
is a blank white viewport containing only the editor axis gizmo. No fireball is present.

### Two independent defects, both real

**1. The system emits nothing (substance, WS-07).** The engine log is unambiguous:

```
[11.55.19:239] LogNiagara: UNiagaraComponent> System /Game/__UeremcpPoc/NS_POCB_Fireball initialized.
[11.55.19:301] LogNiagara: UNiagaraComponent> System /Game/__UeremcpPoc/NS_POCB_Fireball completed.
```

The system reported `completed` 62 ms after `initialized`, i.e. it found no work to simulate and
shut itself down. `changed_pixels=5457` is consistent with the gizmo/actor billboard appearing,
not with particles.

**2. The harness cannot observe particles even in principle (methodology, WS-11).** The gate
captures one frame via `Viewport.Draw()` before spawning and one after, with no world tick in
between; the whole test completed in 236 ms (`11.55.19:130`→`11.55.19:366`). Niagara requires
ticks to spawn and simulate, so a *working* fireball would also have failed this gate. The warm
test (`B.R >= 80 && R*5 >= G*6 && R*3 >= B*4`) additionally cannot fire against the saturated
white background this empty world renders, since additive fire clamps toward white.

### Consequence for prior POC-B claims

B3–B7 asserted emitter/renderer/user-parameter **structure** and material **binding**, and those
assertions remain valid as structural proofs. They did not establish that the system produces
particles. B10 is the first gate to test visible output, and it fails. Until it passes, the POC-B
fireball is unproven as a rendering artifact.

## POC A A1–A11 slice (WS-11, tip lineage `d1eb1ea`→`279f09a`)

| Criterion | Result | Note |
|---|---|---|
| A1 | **SKIP** | One MCP call not proven |
| A2 | **SKIP** | Complete payload fields not all asserted |
| A3 | **PASS** | |
| A4 | **PASS** | |
| A5 | **SKIP** | One MCP call not proven |
| A6 | **PASS** | Dedicated `PocA6Reread` also PASS on `c87b1db` |
| A7 | **PASS** | |
| A8 | **PASS** | |
| A9 | **SKIP** | No MCP round-trip metrics |
| A10 | **SKIP** | `lossy_areas` not asserted |
| A11 | **PASS** | |

At that slice, the aggregate filter was missing. The later CompleteRoundTrip transport result is recorded below; overall POC A remains unclaimed.

## Acceptance runtime follow-up on tip `d691316` (WS-11)

| Proof | Result | Evidence / owner |
|---|---:|---|
| Fireball POC | **FAIL** | `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_050816.log`: Niagara system under POC root; MIs under test root; B4 false; B2 harness manifest-path issue. WS-07/WS-08 own MI co-location. |
| Blueprint `PocA6Reread` | **FAIL** | `editor_UeremcpBlueprint_Toolset_PocA6Reread_20260730_050942.log`: `failed_validation`; BeginPlay→Branch link missing; A8/A11 no-op failed. WS-06 owns fix. |
| Material Toolset | **PASS, 14/14** | `editor_UeremcpMaterial_Toolset_20260730_051149.log`. |

WS-11 parsing fix is integrated as `674c439`. The ordered co-location stack is `58036dd` → `dc4f118`; it does not prove B2/B4 until WS-11 re-runs.

## Acceptance runtime follow-up on tip `c87b1db` (WS-11)

| Proof | Result | Evidence / residual |
|---|---|---|
| Blueprint `PocA6Reread` | **PASS** | `editor_UeremcpBlueprint_Toolset_PocA6Reread_20260730_052810.log`: test Success; editor exit 0. A6 runtime criterion only; not overall POC A. |
| Fireball POC | **FAIL (B4)** | `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_052723.log`: six MIs under POC-root Materials and B2 manifest OK; only `flame_shell` binding verified, five roles failed. |

## Fireball B4 re-run on tip `279f09a` (WS-11)

| Proof | Result | Evidence / residual |
|---|---|---|
| Fireball POC | **FAIL (B4, 5/6)** | `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_053740.log`: `ribbon_trail` MI missing and binding re-read failed; other five roles verified. |
| B4 aggregate honesty | **FAIL** | `B4_material_bindings_verified: true` and `validation.material_bindings_verified: true` covered only five resolved roles. Gate must require all six requested roles. |

## Fireball B2/B4 re-run on tip `a6ca454` (WS-11)

| Proof | Result | Evidence / residual |
|---|---|---|
| Fireball inline materials | **PASS** | `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_055332.log`: all six B4 roles verified; `UEREMCP_POC_B_FIREBALL_OUTCOME=PASS proof=editor_single_create_inline_materials_b2_b4`; exit 0. |

This proves the B2/B4 editor gate only **on that tip**. Later `70cc348` re-run (with `dbb3638`→`7a417bb`, before `ee905ed`) **FAILED** B1/B4 on `ribbon_trail` again — see below. At that point, remaining overall POC-B criteria included B1, B4 freshness, and B8 Create→restart→Verify. B7 had separate scaffold proof; B10 remained required.

## Fireball / B8 / CRT on tip `70cc348` (WS-11; pre-`ee905ed`)

| Proof | Result | Evidence / residual |
|---|---|---|
| Fireball expanded gates | **FAIL (B1/B4)** | `ribbon_trail` MI still absent; B3/B5/B6/B8_save/B9 **PASS** |
| B8 Create | **FAIL** | Same MI / ribbon path failure; restart Verify not reached |
| CompleteRoundTrip | **FAIL overall** | A1–A4/A10 **PASS**; A5/A9 **FAIL** (Python/MCP conflict on SubmitGraph) |

Stacked defenses now on tip: `7a417bb` (`dbb3638`) + `886d09d` (`ee905ed`). Re-run fireball/B8 on `01b9320` after WS-08 confirms `FireballRibbonTrailPoc`. No overall POC-B claim.

## Fresh-DLL fireball re-run after `886d09d` (WS-11)

| Proof | Result | Evidence / residual |
|---|---|---|
| Fireball expanded gates | **FAIL (`ribbon_trail`)** | `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_063745.log` |
| B8 | **SKIPPED** | Fireball remained blocked; WS-08 trail graph / `FireballRibbonTrailPoc` is critical |

Both stale-master defenses were present in the rebuilt DLLs, so stale binaries no longer explain the failure. Trail UV fix later landed as `cf7e6d3` (`2187d69`); fireball/B8 must re-run. No overall POC-B claim.

## Post-UV fireball editor gate (`2187d69` / orch `cf7e6d3`)

| Proof | Result | Evidence / residual |
|---|---|---|
| `FireballInlineMaterials` | **PASS** | `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_064451.log`: all six MIs, including `ribbon_trail`; B1/B3/B5/B6/`B8_assets_saved`/B9 gates and aggregate outcome PASS |
| `FireballRibbonTrailPoc` | **PASS** | Isolated trail proof passed after loading `2187d69` |

This closes the editor fireball/trail UV blocker and supplies current B2/B4
evidence. The filter's B1 is a direct editor single-create pipeline, **not** proof
that one MCP transport request produced the effect. `B8_assets_saved` proves the
save half only; POC_ACCEPTANCE B8 still requires WS-11
Create→restart→Verify. **Overall POC B remains unclaimed.**

## WS-11 freshness and restart proof on tip `8a8c75d`

| Proof | Result | Evidence / residual |
|---|---|---|
| `FireballInlineMaterials` | **PASS** | `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_064554.log`: editor gates B1–B6/B9 green; all six B4 roles, including `ribbon_trail` |
| B8 Restart Create | **PASS** | `editor_UEREMCP_Niagara_POCB_Restart_Create_20260730_064653.log` |
| B8 Restart Verify | **PASS** | `editor_UEREMCP_Niagara_POCB_Restart_Verify_20260730_064733.log`: `restart_observed` and `reread_after_restart` for all ten checkpoint assets |

Acceptance interpretation remains strict: the editor filter's B1 field proves one
direct create pipeline, not POC_ACCEPTANCE B1's explicit one-MCP-request transport
requirement. B10 is **required**, because it is a numbered criterion under the
document's binary acceptance rules; “screenshot as supplementary evidence only”
means a screenshot cannot itself be the validation, not that B10 is optional.
Global POC-B metrics and the equivalent primitive-call baseline also remain
unrecorded. **Overall POC B remains unclaimed.**

## MCP B1 attempt on orch `3b69e8f`

| Proof | Result | Evidence / residual |
|---|---|---|
| Canonical one-request MCP fireball | **FAIL (editor crash)** | `CreateNiagaraEffect` with six materials and validation enabled crashed at `UeremcpNiagaraCreate.cpp:589` in `AwaitCompile` |
| Editor `FireballInlineMaterials` | **PASS** | Existing editor proof remains green; it is not MCP transport B1 |
| B10 / metrics | **BLOCKED** | Await WS-07 crash fix and successful WS-11 MCP rerun |

No B1, B10, metrics, or overall POC-B claim.

WS-07 fixed the crash in `132bb54`, landed as orch `088bd64`: compile waiting
is poll-only and no longer reenters game-thread draining. Niagara rebuilt
successfully. WS-11 must rerun `poc_b_mcp_fireball_request.json`; the fix/build
alone does not pass B1.

The WS-11 rerun on orch `8322ee6` with `088bd64` loaded still **FAILS**:
`AwaitCompile` reaches a SharedPointer `IsValid` assertion at
`UeremcpNiagaraCreate.cpp:593`. The editor fireball remains PASS. WS-07 is
investigating further; B10 and metrics remain blocked.

WS-07 then landed `9c9b9b4` as orch `79d9d65`: live MCP dispatch requests
compilation without invoking compile-completion polling, while automation keeps
the poll path. Niagara rebuilt successfully. WS-11 must rerun the canonical
fixture; a crash-free JSON response is the immediate proof target. An honest
`partially_completed` response does not by itself pass B1/B6 or overall POC B.

The WS-11 rerun is now **crash-free** and returned JSON:
`status: partially_completed`, `metrics.mcp_round_trips: 1`,
`B1_single_request_complete: false`, `B6_compile_awaited: false`, and
`checks_skipped: niagara.compile_await_deferred_tool_dispatch`. This closes the
crash residual only. B1/B6 remain failed for the end-to-end MCP scenario.

WS-07 owns a safe compile-complete path that avoids the crashing query. ADR-0009
job completion is the other accepted architecture path, but any MCP poll calls
must count as additional round trips and therefore do not satisfy B1's current
“no follow-up calls” criterion.

WS-07 `4dc53b7` then landed as orch `d07f8f1`. The MCP path now observes
script compile state without the crash-inducing completion query. Niagara rebuilt
cleanly (DLL 07:19:11).

## MCP B1/B6 on orch `73b930e` (`d07f8f1` loaded)

| Proof | Result | Evidence / residual |
|---|---|---|
| Canonical one-request MCP fireball | **B1 PASS / B6 PASS** | `mcp_round_trips=1`; single-request pipeline true; compile awaited true; six materials present |
| Status | `partially_completed` | Honest — B10 still skipped |
| Metrics | **Partial** | Round trips=1, internal ops=46; 2.319s server-side lower bound (not wall time); client wall / tokens / primitive baseline outstanding |

B1–B9 are covered by editor + MCP + restart evidence. Remaining for overall
POC B: **B10** and **complete metrics/baseline**. No overall POC-B claim.

## B10 filter landing on orch `01b257e`

| Proof | Result | Evidence / residual |
|---|---|---|
| Harness unit tests | **PASS 3/3** | `python -m unittest tests.unit.test_poc_b10_visible_render_harness` |
| `UeremcpValidation` rebuild | **FAIL** | `NiagaraPocBVisibleRender.spec.cpp:131-132`: viewport-client type mismatch and missing `GetViewport` member `[VERIFIED-RUNTIME: UeremcpValidation build on 2026-07-30]` |
| B10 runtime filter | **NOT RUN** | New DLL was not produced; WS-11 fix/rebuild required |

B10 remains unproven; WS-11's fix/rebuild/run is in flight. Metrics handoff
`41f4f82` records MCP round trips=1, internal operations=46, and a reproducible
2.319s server-side lower bound that must **not** be reported as wall time. Client
wall-clock, total tokens, and the measured equivalent primitive baseline remain
open for the WS-07/WS-11/WS-14 rerun handoff. WS-07 `b623084` landed as
`f295eb8`, adding `metrics.timing_ms` stages and a primitive baseline **sequence**
without inventing counts. Niagara rebuilt successfully; the MCP timing and
equivalent baseline trials have not yet run.

## CompleteRoundTrip on tip `3756244` — overall POC A

| Proof | Result | Evidence |
|---|---|---|
| A1–A11 | **ALL PASS** | `tests/integration/_logs/poc_a_complete_round_trip_3756244.json` |
| Metrics | 3 MCP / 4 internal / 2.30s / 0 errors | Same file |

**Overall POC A CLAIMED** for the demonstrated simple-graph CRT scenario (native `EventBeginPlay→Branch→PrintString` slice; honest A10 lossy_areas). Not a claim for arbitrary complex Blueprint graphs.

## Blueprint CompleteRoundTrip current result

Prior transport run on `600c383` / `70cc348`: **FAIL overall** (A5 blocked). Superseded by `3756244` PASS above.

## B8 restart current result

**PASS on `8a8c75d`:** WS-11 Create→restart→Verify observed a fresh editor
process and re-read all ten checkpoint assets. This satisfies B8. It does not
satisfy B1, B10, or the global POC-B metrics requirements.

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
| WS-06 | Overall POC A claimed via Python-free CRT on `3756244`; support only if regressions appear. |
| WS-07 | Production fireball now reports **185 particles** under the fixed B10 harness but **0 warm pixels**. Finish create-path runtime scaffold (`bAllowSystemStateFastPath=false`, remove template emitters, spawn/color/renderer visibility) until B10 warm signature appears; keep B1–B9 structural gates green. |
| WS-08 | Stand by: engage only if WS-07 proves bound materials are invisible (opacity/emissive/blend) rather than particle attributes/renderers. |
| WS-10 | Animation Toolset PASS 10/10 on `5ea9277`; no further Animation filter work from this triage. |
| WS-11 | B10 methodology closed on `0049153` (canary PASS). Production still FAIL. Next after visible fire: MCP `timing_ms` rerun + WS-07 primitive baseline with WS-14 metrics capture. |
| WS-15 | Templates PASS 4/4 on `f15ea96`; no remaining Templates filter failure in this record. |

**Overall POC A claimed** on CRT `3756244`. MCP B1/B6 and editor B2–B9 remain PASS as structural proofs. B10 harness can observe particles (`0049153` canary PASS). Production fireball still fails warm signature (`185` particles, `0` warm). Complete metrics/baseline remain open. No overall POC-B claim. No junction retarget.

---

## Pointer — acceptance-gap audit (2026-07-30)

Canonical gap list: [`ws-01-acceptance-gap-audit-2026-07-30.md`](./ws-01-acceptance-gap-audit-2026-07-30.md).

**Current tip reality (do not conflate worktrees):**

| Fact | Value |
|---|---|
| B10 harness canary | **PASS** on `0049153` |
| B10 production fireball | **FAIL** (0 warm pixels; log `081341` / `poc_b10_fireball.png`) |
| Tip lineage on `ws-11-poc-b10-render` | this commit (audit landed on `7b654f4`+) |
| Concurrent WS-07 Niagara WIP | may exist uncommitted on `ws-07-niagara-runtime-spawn` — do not assume tip parity |

No overall POC-B claim.
