# WS-07: Niagara deferred gaps — engine blockers (honest)

**From:** WS-07 (Niagara) on `ws-07-08-material-niagara-integration`  
**Date:** 2026-08-05

## Shipped this session

1. **SimTarget + Life Cycle R/W** — `emitters[].sim_target` via `SetEmitterData` PropertyValues; `life_cycle` / `loop_duration` via Emitter State `SetStackInputData`. Create / Submit / Adapt / Inspect.
2. **Linked / DI / HLSL / dynamic / enum inputs** — Create+Submit `inputs{}` via `FNiagaraExt_StackInputData_*` + `SetStackInputData`.
3. **Event handlers READ** — `GetEventHandlers()` metadata + `UNiagaraScriptSource::NodeGraph` module samples on Inspect.
4. **Script graph READ summary** — node counts / sample function calls / custom HLSL count; `write_supported=false`.
5. **Hash round-trip harness** — `EvaluateRetrieveSubmitRetrieveStability`; flips `round_trip_supported` only when proven (stays false offline).

## Precise engine blockers (still)

### Event handler WRITE

`UNiagaraExternalEditUtilities` cannot address `ParticleEventScript` stacks:

- `FNiagaraExt_StackItemReference` has `ScriptName` only — **no UsageId / Guid field**  
  [VERIFIED: NiagaraExternalSystemEditorUtilities.h:944-998]
- `FindScriptGroup(Usage, ScriptUsageId)` requires Guid match; empty Guid fails for event handlers  
  [VERIFIED: NiagaraStackQuery.cpp:160-182]
- No `AddEventHandler` on `UNiagaraExternalEditUtilities` (UI uses `FNiagaraEmitterViewModel::AddEventHandler`)

**Consequence:** Create/Submit cannot add/enable event modules via ExternalEditUtilities. Documented in capability_notes + checks_skipped `niagara.event_handler_stacks_write`.

### Script graph WRITE

`UNiagaraScriptSource::NodeGraph` is a public UPROPERTY for read, but ExternalEditUtilities has **no EdGraph mutate API**. Custom HLSL / pin rewires remain out of agent surface.

### round_trip_supported

Remains **false** until a live retrieve→submit→retrieve pass proves hash stability. Offline harness proves the flip logic only.

## Catalog / GetStarted

No WS-01 catalog edits in this pass (ownership). Notes updated in-module; propose catalog refresh when convenient.
