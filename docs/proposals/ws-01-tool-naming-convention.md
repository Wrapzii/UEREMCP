# Tool naming convention (BACKLOG 1.4)

**Status:** Accepted for implementation (aliases, not rename). **Owner:** WS-01.  
**Date:** 2026-07-30.

## Decision

1. **Canonical agent-facing names for UEREMCP C++ tools remain PascalCase
   UFUNCTION names** (`CreateNiagaraEffect`, `BuildEnvironment`, `GetStarted`).
   These are what `describe_toolset` / ToolsetRegistry emit.
2. **Do not mass-rename to snake_case.** That would break every live client and
   six workstreams for zero semantic gain.
3. **Normalization/aliases belong in ResolveIntent + docs**, not in dual
   registration of every tool:
   - Intent router already ranks on description vocabulary and catalog aliases.
   - `tools/check_tool_names.py` treats near-miss Pascal/snake confusion as a
     CI failure when docs claim a non-existent qualified name.
4. **Epic Python toolsets keep their own snake_case.** Agents must not assume one
   convention across the whole 900-tool surface — that is why GetStarted /
   ResolveIntent exist.

## Migration

No breaking rename. Optional future: IntentRouter accepts
`specification.tool_alias` snake_case → PascalCase map for UEREMCP-only tools.
Not required to close BACKLOG 1.4.
