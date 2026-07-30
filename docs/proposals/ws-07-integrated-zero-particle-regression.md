# WS-07 integrated zero-particle regression

## Scope

This record diagnoses and fixes the fresh-create Niagara runtime regression on the
integrated B10 lineage. It does not claim overall POC-B completion and does not modify
the WS-11 B10 harness.

## Source-lineage check

`git diff 8aae3b6 20edf2f -- Plugins/UEREMCP/Source/UeremcpNiagara` is empty. The
Niagara source from the original emission-proof commit landed byte-identically.

The earlier 717 spawned / 423 live observation therefore did not prove that a newly
generated, saved, and reloaded production asset had a normalized system lifecycle and
fully refreshed compiled runtime state.

## Root cause

The cloned system template authored `SystemState` as `Once` with a zero-second loop
duration. `PrepareRuntimeScaffold` disabled the system-state fast path but did not
rewrite those graph inputs. The runtime consequently completed before emitter spawn
scripts could run. [VERIFIED: NiagaraSystemInstance.cpp:3156-3217]

The generated role emitters are stateful. UE 5.8's fast-path resolver declines systems
containing any enabled non-stateless emitter, so enabling
`bAllowSystemStateFastPath` cannot repair this asset shape.
[VERIFIED: NiagaraSystemStateDataResolver.cpp:227-236]

Stack edits also require an actual compile and post-compile cache rebuild before save.
The previous non-forced request could report the scripts as already current while
retaining stale runtime data. Niagara's post-compile path rebuilds system/emitter
compiled data and resolves runtime state. [VERIFIED: NiagaraSystem.cpp:3608-3621]

Materials were not the cause. The zero-particle runs had no runtime emitter instances,
whereas the repaired run creates all six instances and counts spawned particles.
[VERIFIED-RUNTIME: UEREMCP.Niagara.Create.PocBParticlesSpawn rendered run
editor_UEREMCP_Niagara_Create_PocBParticlesSpawn_20260730_090852.log]

## Generator fix

- Normalize the cloned `SystemState` module to `Infinite` with a non-zero loop duration.
- Keep `bAllowSystemStateFastPath=false` for the stateful generated emitter set.
- Force Niagara compilation after stack edits so the saved package contains refreshed
  runtime data.
- Continue removing the template emitter. Runtime diagnostics showed the template
  `Fountain` emitter was not ready to run and made the whole system unready, while all
  six generated role emitters were ready.
  [VERIFIED: NiagaraSystem.cpp:1774-1790]

## Runtime evidence

Fresh production create followed by the rendered 3-second probe:

```text
UEREMCP_NIAGARA_RUNTIME_EVIDENCE={"emitters":6,"enabled_emitters":6,
"spawn_modules":5,"initialize_modules":6,"live_particles":421,
"total_spawned_particles":717,"runtime_emitter_instances":6,
"compiled_emitter_data":6,"system_valid":true,"system_ready_to_run":true,
"component_complete":false}
```

All six role emitters separately reported `ready=true`.
[VERIFIED-RUNTIME: rendered automation log
tests/integration/_logs/editor_UEREMCP_Niagara_Create_PocBParticlesSpawn_20260730_090852.log]

The same test under the default `-NullRHI` runner cannot be a valid Niagara runtime
proof: UE 5.8 returns false from `IsReadyToRunInternal()` whenever
`FApp::CanEverRender()` is false. [VERIFIED: NiagaraSystem.cpp:1731-1737]

## B10 result and remaining gap

The fresh asset was loaded by B10, but the unchanged WS-11 harness still reported:

```text
UEREMCP_POC_B10_OUTCOME=FAIL reason=system_emits_no_particles
particle_count=0
```

[VERIFIED-RUNTIME: tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_090931.log]

This differs from the rendered runtime probe of the same saved package. The known B10
world-tick defect remains outside WS-07 ownership; no B10 harness changes were made.
The warm-signature question remains secondary until WS-11 reruns with a ticking world.
