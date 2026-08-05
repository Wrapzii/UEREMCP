# Proposal: GetStarted niagara_path_policy Magecraft write (WS-07 → WS-03)

**From:** WS-07  
**To:** WS-03 (`Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpIntentRouter.cpp`)  
**Date:** 2026-08-05  

## Current (stale)

`GetStarted` still says CreateNiagaraEffect WRITE is sandbox-only (`NIAGARA_MUTATE_SANDBOX_ONLY`).

## Required replacement text for `niagara_path_policy`

```
Prefer UeremcpNiagara.InspectSystem for READ of any /Game/… including Magecraft
(one-shot result.graphs[] + emitters + user_parameters) — do not browse Epic
NiagaraToolsets for that. CreateNiagaraEffect WRITE under /Game/__UeremcpTests,
/Game/__UeremcpPoc, or /Game/RE/VFX/Magecraft. AdaptNiagaraEffect for in-place
User.*/materials on existing Magecraft/sandbox systems (never deletes).
mode=replace delete is sandbox-only. CaptureEffectFrames for visual proof.
fidelity.round_trip_supported remains false until graph JSON submit lands.
```

See also `docs/proposals/ws-07-niagara-magecraft-mutate-envelope.md`.
