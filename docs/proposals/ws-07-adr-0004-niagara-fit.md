# WS-07 note: ADR-0004 fit for Niagara module stacks (no challenge)

- **From:** WS-07
- **To:** WS-01 / WS-14
- **Date:** 2026-07-29
- **Related:** `docs/research/RB-07-niagara.md` R-05 verdict

## Decision

**No ADR-0004 challenge.** Niagara module stacks map to existing
`graph_type` values (`NiagaraModuleStack`, `NiagaraEmitterGraph`,
`NiagaraSystemGraph`, `NiagaraScriptGraph`) plus `extensions.niagara`.

## Evidence (short)

Epic's AICallable topology model is already an ordered module list per script
usage with typed stack inputs — not a Blueprint pin graph
`[VERIFIED: NiagaraExternalSystemEditorUtilities.h FNiagaraExt_EmitterTopology]`.
That maps cleanly to `nodes[]` + optional sequential exec `links[]`, with value
modes / dynamic chains / renderer blobs / event-handler gaps in
`extensions.niagara`.

## Known lossy areas (will appear in `fidelity.lossy_areas`)

- Event handler stacks not returned by `GetEmitterTopology`
- No public ReorderModule tool
- Module script EdGraphs out of POC scope

Full write-up: `docs/research/RB-07-niagara.md`.

## Response (WS-01)

**Accepted — no ADR-0004 challenge.** R-05 fork risk marked **mitigated**. Residual
lossy areas stay in domain `fidelity` / capability notes. Wave 2
`UeremcpNiagara` waits on Phase 1 exit.
