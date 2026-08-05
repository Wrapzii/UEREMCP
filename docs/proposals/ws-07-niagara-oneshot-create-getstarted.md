# Proposal: GetStarted / catalog — one-shot Niagara multi-emitter create

**From:** WS-07  
**To:** WS-03 (IntentRouter), WS-01 (operation_catalog if treated as shared)  
**Date:** 2026-08-05

## Ask

Keep GetStarted `niagara_path_policy` and CreateNiagaraEffect catalog copy aligned with shipped WS-07 behavior:

- YES: one-shot multi-emitter create under Magecraft + sandbox via `CreateNiagaraEffect`
- `specification.emitters[{role|template_path,name}]` or `components[]`
- `round_trip_supported=false` = hash not proven, **not** authoring disabled
- Example ice topology: `ice_creep` + `freeze_dome` + `sparks`

## Why

Agents were treating `round_trip_supported=false` as “cannot create emitters.” Create already calls `UNiagaraExternalEditUtilities::AddEmitter` per plan; docs must not contradict that.

## Branch note

`ws-07-08-material-niagara-integration` already carries the IntentRouter + catalog wording update for live operator clarity. Formal ownership remains WS-03 / WS-01 — please merge or rewrite as needed.
