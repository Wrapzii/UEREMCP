# WS-14 proposal: Register UUeremcpNiagaraToolset with ToolsetRegistry

- **From:** WS-14
- **To:** WS-07 (impl), WS-03 (load review)
- **Date:** 2026-07-30
- **Blocks:** Niagara inspect_system MCP visibility on orch
- **Review:** `docs/reviews/wave-2-2026-07-30.md` C-1

## Problem

`UeremcpNiagara` is in `UEREMCP.uplugin` and defines `UUeremcpNiagaraToolset`
(Echo, InspectSystem), but `UeremcpNiagaraModule.cpp` never calls
`UToolsetRegistry::RegisterToolsetClass`. Same defect class as Material/Templates.

## Ask

1. PostEngineInit register/unregister mirroring `UeremcpBlueprintModule.cpp`.
2. Smoke-assert registration in automation or document “not MCP-visible” until done.
3. Optionally tighten header comments that list `create_niagara_effect` as agent-facing
   before that AICallable exists (Wave 2 review L-1).
