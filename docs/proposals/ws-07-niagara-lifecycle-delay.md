# WS-07 — Full Emitter State Life Cycle / Timeline in one envelope

**Status:** Implemented on `ws-07-niagara-lifecycle-delay`  
**Owner:** WS-07 (`Plugins/UEREMCP/Source/UeremcpNiagara/**`, `schemas/domains/niagara/**`)

## Problem

Agents could set `life_cycle.loop_duration` / `loop_behavior` / `inactive_response` but
**not** Timeline Start. Every emitter landed at `Start=0.00`. Separately, Loop Duration /
Loop Delay are gated behind Life Cycle Mode = Self — writing them first produced
"Refusing to set input…" warnings.

## Verified engine map

| JSON key | Emitter State input | Timeline binding |
|---|---|---|
| `mode` | `Life Cycle Mode` | gates Self panel |
| `loop_behavior` | `Loop Behavior` | gates Loop Count |
| `loop_duration` | `Loop Duration` | `TimelineInputUsage=Length` |
| `delay` / `start_time` / `loop_delay` | `Loop Delay` | **`TimelineInputUsage=StartTime` — moves Timeline Start column** |
| `use_loop_delay` / `use_start_time` | `UseLoopDelay` | `TimelineInputUsage=UseStartTime` (auto-true when delay set) |
| `delay_first_loop_only` | `Delay First Loop Only` | `StartTimeIncludedInFirstLoopOnly` |
| `loop_count` / `num_loops` | `Loop Count` | `NumLoops` |
| `loop_duration_mode` | `Loop Duration Mode` | — |
| `recalculate_duration_each_loop` | `Recalculate Duration Each Loop` | — |
| `inactive_response` | `Inactive Response` | — |

Citations:

- `[VERIFIED: NiagaraConvertInPlaceEmitterAndSystemState.cpp:80-210]` input display names
- `[VERIFIED: MovieSceneNiagaraEmitterSection.cpp:25-31,305-308]` Timeline Start ← StartTime usage ← Loop Delay
- `[VERIFIED-RUNTIME: poc_b EmitterState input_pins]` live pin dump

`emitters[].enabled` / `sim_target` remain first-class sibling fields (SetEmitterData).

## Apply order

1. `Life Cycle Mode` (auto `Self` when duration/delay/count present and mode omitted)
2. `Loop Behavior`
3. `UseLoopDelay`
4. `Loop Duration Mode`
5. `Inactive Response`
6. numeric/bool values (`Loop Duration`, `Loop Delay`, `Loop Count`, …)

## Agent path (primary)

`CreateNiagaraEffect` / `SubmitNiagaraGraph` / `AdaptNiagaraEffect` with fat
`emitters[].life_cycle{}` — **not** Epic `SetStackInputData` drip-feed.

Epic `NiagaraToolsets.*` / raw `SetStackInputData` is a **temporary fallback only** until
the editor is cold-rebuilt with this plugin binary. After rebuild, prefer UEREMCP.

## Fixture

`schemas/domains/niagara/fixtures/create_staggered_cast_life_cycle.json` —
HandCharge delay 0 → BoltCore 0.3 → ReleaseFlash 0.8 in one Create specification.
