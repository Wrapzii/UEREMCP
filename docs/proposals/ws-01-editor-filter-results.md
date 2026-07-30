# WS-01 editor automation filter results

- **Current orchestration tip:** `1b228b7` (`[WS-07] Fix B7 gate scaffold create rejection`)
- **Runtime result tip:** `fcdf2e5` (`[WS-15] Execute template modifiers and validation rules`)
- **Date:** 2026-07-30
- **Status:** Mixed. Compile success did not imply runtime gate success.
- **Junction:** Not changed.

## Invocation

Each suite used the established runner:

```powershell
pwsh -NoProfile -File "tests/run_editor_tests.ps1" -KeepUeremcp -NoProbe -Filter "<filter>"
```

The runner launched `UnrealEditor-Cmd.exe` with `-unattended -nop4 -nosplash -NullRHI -nosound` and `Automation RunTests <filter>; Quit`. Foreign-platform SDK setup warnings remained visible but were nonfatal; automation discovery and execution started.

## Failing suites

| Filter | Passed | Failed | First failing test | First failure evidence |
|---|---:|---:|---|---|
| `UEREMCP.Niagara.Inspect` | 3 | 1 | `UEREMCP.Niagara.Inspect.NS_WS07_Probe` | Expected status `partially_completed`, but received `rejected`; diagnostics were also absent. |
| `UEREMCP.Niagara.Create` | 8 | 2 | `UEREMCP.Niagara.Create.MaterialBindingOffline` | Expected unresolved `create_spec` count `0`, but received `1`. |
| `UeremcpMaterial.Toolset` | 3 | 7 | `UeremcpMaterial.Toolset.CreateProceduralTexture.FlipbookAtlas` | Expected `created_and_validated`, but received `failed_validation` (reported atlas width/height were `0`, expected `256`). |
| `UeremcpBlueprint.Toolset` | 2 | 2 | `UeremcpBlueprint.Toolset.ReadGraphRoundTrip` | Summary and complete revisions differed: expected `sha256:213b9f3f...`, received `sha256:9d7480e2...`. |
| `UEREMCP.Animation` | 5 | 5 | `UEREMCP.Animation.InspectMontage.EnvelopeRejections` | The malformed-request rejection assertion was false (wrong-action and missing-target rejection assertions also failed). |

### Additional failed tests

- `UEREMCP.Niagara.Create.ReplaceDryRun`
- `UeremcpMaterial.Toolset.CreateProceduralTexture.Noise`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.Distortion`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.FlipbookSubuv`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.ValidateFalse`
- `UeremcpBlueprint.Toolset.SubmitGraphValidation`
- `UEREMCP.Animation.InspectMontage.NotifyOrdering`
- `UEREMCP.Animation.InspectMontage.StructuredState`
- `UEREMCP.Animation.ReadAnimBp.EditorScratchAsset`
- `UEREMCP.Animation.ReadAnimBp.EnvelopeRejections`

## Passing suites

| Filter | Result |
|---|---:|
| `UEREMCP.Niagara.PlanHandlers` | PASS, 3/3 |
| `UEREMCP.Material.PlanHandlers` | PASS, 3/3 |
| `UEREMCP.Transport.Handoff` | PASS, 1/1 |
| `UEREMCP.Protocol.PlanExecutor` | PASS, 7/7 |

`UEREMCP.Templates` was not run: no Templates automation filter registration was present in the source search at the result tip. This is recorded as **SKIP / unavailable**, not PASS.

## Evidence logs

- `tests/integration/_logs/editor_UEREMCP_Niagara_Inspect_20260730_021342.log`
- `tests/integration/_logs/editor_UEREMCP_Niagara_Create_20260730_021408.log`
- `tests/integration/_logs/editor_UeremcpMaterial_Toolset_20260730_021434.log`
- `tests/integration/_logs/editor_UeremcpBlueprint_Toolset_20260730_021501.log`
- `tests/integration/_logs/editor_UEREMCP_Niagara_PlanHandlers_20260730_021524.log`
- `tests/integration/_logs/editor_UEREMCP_Material_PlanHandlers_20260730_021546.log`
- `tests/integration/_logs/editor_UEREMCP_Transport_Handoff_20260730_021216.log`
- `tests/integration/_logs/editor_UEREMCP_Animation_20260730_021236.log`
- `tests/integration/_logs/editor_UEREMCP_Protocol_PlanExecutor_20260730_021259.log`

## Handoff note

These results predate the current `1b228b7` B7 fix. Do not treat the Niagara failure rows as verification of that fix; WS-11 owns the B7 re-run. No editor retake was performed while that re-run held priority.