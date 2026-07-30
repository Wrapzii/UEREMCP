# WS-01 editor automation filter results

- **Current orchestration tip:** `4d5e304` (`[WS-01] Record Animation Toolset PASS 10/10`)
- **Latest Animation re-run tip:** `5ea9277`
- **Latest Niagara re-run tip:** `b5b07e1`
- **Latest Material re-run tip:** `c881742`
- **Prior mixed re-run tip:** `c234606`
- **Date:** 2026-07-30
- **Status:** Material, Animation, and Niagara Inspect green; Niagara B7 fails during edit-condition logging/cleanup. No A6 / POC-B completion claims.
- **Junction:** Not changed.

## Invocation

Each suite used the established runner:

```powershell
pwsh -NoProfile -File "tests/run_editor_tests.ps1" -KeepUeremcp -NoProbe -Filter "<filter>"
```

The runner launched `UnrealEditor-Cmd.exe` with `-unattended -nop4 -nosplash -NullRHI -nosound` and `Automation RunTests <filter>; Quit`.

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
| WS-07 | Fix B7 `bOverrideMaterials` edit-condition error and `NS_POCB_FireballProbe` cleanup ensure; Inspect is PASS 4/4. |
| WS-08 | Material Toolset PASS on `c881742` — no further Material filter work from this triage. |
| WS-10 | Animation Toolset PASS 10/10 on `5ea9277`; no further Animation filter work from this triage. |
| WS-11 | Re-run B7 after WS-07 fix; keep POC-B claims gated to their own criteria. |

Standing by on orch for the WS-07 B7 fix. No junction retarget.
