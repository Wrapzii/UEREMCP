# Proposal: promote UEREMCP Niagara probes into `/Game/RE/VFX/...`

**From:** post-cast spell VFX lane (ChainLightning residual / Frozen Wall templates)  
**Problem:** `CreateNiagaraEffect` correctly refuses `/Game/RE/...` (probe roots only). Agents author under `__UeremcpTests`, but production routines need `/Game/RE/VFX/Magecraft/Spells/`.

## Options (do not silently expand roots)

1. **New semantic action** `PromoteNiagaraProbe` (preferred): copy/migrate a validated probe from `__UeremcpTests` → allowed RE VFX dest with content_hash check + save. Destructive outside dest = false; dest must be under `/Game/RE/VFX/`.
2. **One-shot editor move** after `created_and_validated` — human or REAgentTools composite; not Python `duplicate_asset` authoring.
3. **Expand probe roots** to include `/Game/RE/VFX/Magecraft/` — rejected unless ADR; breaks AGENTS “never destroy user content” / probe isolation.

## Interim

- Author with `include_adaptation: true` under `/Game/__UeremcpTests/Magecraft/...`
- C++ `FRESpellVFXAdaptation` stamps User.* at spawn even if vars missing (Niagara accepts SetVariable on undeclared names softly — still prefer declared)
- Routines JSON may point at DissipateMist until promote lands

## Acceptance

- Probe survives disk restart
- Promote produces production path once
- CaptureEffectFrames on promoted asset
