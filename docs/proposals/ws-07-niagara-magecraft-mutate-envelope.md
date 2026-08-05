# Proposal: Niagara Magecraft mutate envelope + GetStarted (WS-07 → WS-01 / WS-03)

**From:** WS-07  
**To:** WS-01 (`schemas/envelope/**`), WS-03 (`UeremcpCore` IntentRouter / GetStarted)  
**Date:** 2026-08-05  

## Need

UEREMCP Niagara now authorizes WRITE under `/Game/RE/VFX/Magecraft/**` for
`create_niagara_effect` (new assets) and `adapt_niagara_effect` (in-place). Destructive
`mode=replace` / delete remain sandbox-only. Agents and GetStarted still advertise the old
“sandbox-only write” policy, which steers them to Epic NiagaraToolsets.

## Ask WS-01

Add stable error codes to `schemas/envelope/response.schema.json`:

- `NIAGARA_MUTATE_PATH_DENIED` — write outside sandbox + Magecraft
- `NIAGARA_MUTATE_NO_REPLACE_ON_PRODUCTION` — `mode=replace` on Magecraft

Keep `NIAGARA_MUTATE_SANDBOX_ONLY` as a deprecated alias or remove after clients migrate.

## Ask WS-03

Update `FUeremcpIntentRouter::GetStarted` `niagara_path_policy` to:

1. Prefer `UeremcpNiagara.InspectSystem` for READ of any `/Game/…` (one-shot `result.graphs[]`).
2. Prefer `CreateNiagaraEffect` / `AdaptNiagaraEffect` for WRITE under Magecraft or sandbox.
3. Never prefer Epic `NiagaraToolsets.*` for Magecraft discover/author loops.
4. Note `mode=replace` delete is sandbox-only; use adapt for existing Magecraft systems.
5. Note `fidelity.round_trip_supported=false` until graph JSON submit lands.

## WS-07 status (already shipping in owned paths)

- Path helpers + Adapt tool + Inspect query resolve + capability notes updated.
- Cursor rule `mcp-bootstrap.mdc` needs matching text (unowned / WS-01 — see sibling note).
