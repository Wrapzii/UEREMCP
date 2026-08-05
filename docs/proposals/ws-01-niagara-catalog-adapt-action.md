# Proposal: operation_catalog adapt_niagara_effect (WS-07 → WS-01)

**From:** WS-07  
**To:** WS-01 (`tools/intent_router/operation_catalog.json`, Content mirror)  
**Date:** 2026-08-05  

## Need

Add catalog entries so ResolveIntent / GetStarted discover:

| Field | Value |
|---|---|
| toolset | `UeremcpNiagara.UeremcpNiagaraToolset` |
| tool | `AdaptNiagaraEffect` |
| action | `adapt_niagara_effect` |
| when | in-place User.*/material edits on Magecraft or sandbox |
| next | `inspect_system`, `CaptureEffectFrames` |

Also update existing `create_niagara_effect` / `inspect_system` entries:

- create paths: sandbox **or** `/Game/RE/VFX/Magecraft`
- inspect: one-shot `result.graphs[]`; optional `specification.query`
- demote any Epic NiagaraToolsets recommend for Magecraft authoring

Schema: `schemas/domains/niagara/adapt_niagara_effect.schema.json` (already landed).
