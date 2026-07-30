# WS-14 proposal: Register UUeremcpTemplatesToolset with ToolsetRegistry

- **From:** WS-14
- **To:** WS-15 (impl), WS-03 (load review)
- **Date:** 2026-07-30
- **Blocks:** search_templates / instantiate_template MCP visibility on orch
- **Review:** `docs/reviews/wave-2-2026-07-30.md` C-1

## Problem

`UeremcpTemplates` loads the JSON store on startup (`UeremcpTemplatesModule.cpp`) and
defines `UUeremcpTemplatesToolset` with SearchTemplates / InstantiateTemplate, but never
calls `UToolsetRegistry::RegisterToolsetClass`. Python unit tests pass; MCP agents still
cannot call the tools.

## Ask

1. PostEngineInit register/unregister mirroring Blueprint.
2. After registration, instantiate_template may remain `partially_completed` (plan only) —
   that status is already honest; visibility is the gap.
3. Add a note in Templates README: module load ≠ toolset registration.
