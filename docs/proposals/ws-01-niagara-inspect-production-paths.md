# Proposal: Niagara inspect/mutate path split (Magecraft confusion)

- **From:** WS-01 (user-directed fix)
- **Owns code:** WS-07 (`UeremcpNiagara`), WS-03 (`GetStarted` note), WS-01 (`schemas/envelope`)
- **Date:** 2026-08-04

## Problem

Agents treated the live bridge as “sandbox-only” and refused to inspect or prove
production Magecraft spells. Mutate **was** sandbox-only; inspect was incorrectly
gated the same way, so agents concluded they could do nothing useful on
`/Game/RE/VFX/Magecraft/**`.

## Fix landed (inspect split)

1. **Inspect** → any `/Game/…` path (production Magecraft OK)
2. Loud capability notes + inspect path split
3. `GetStarted` payload field `niagara_path_policy` (needs Magecraft-write refresh — see superseding proposal)

## Superseded for mutate (2026-08-05)

WS-07 now allows **create/adapt under Magecraft**; destructive `mode=replace`/delete stay sandbox-only.
See `docs/proposals/ws-07-niagara-magecraft-mutate-envelope.md` and
`docs/proposals/ws-03-niagara-getstarted-magecraft-write.md`.

## Agent routing (do not require Epic defaults)

Prefer `UeremcpNiagara.InspectSystem` for Magecraft/project **reads**. Epic
`NiagaraToolsets.*` are composed internally — not the agent-facing path. Epic is
last-resort only when ResolveIntent abstains / no Ueremcp action exists.

WS-07: please adopt/review. WS-13: mirror in `docs/guide/tool-selection-policy.md` when free.
Do **not** document Epic Niagara as required for production inspect.
