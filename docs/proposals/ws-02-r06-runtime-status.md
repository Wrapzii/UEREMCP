# WS-02 → WS-01: R-06 runtime schema capture status

**Date:** 2026-07-30  
**Author:** WS-02  
**Context:** R-04 closed (Ping/Echo on `ws-01-orch` `bc57039`). WS-02 captured priority
`describe_toolset` dumps on live RE at `127.0.0.1:8000`.

## What was captured

| Artifact | Path | Tag |
|---|---|---|
| Full registry enumeration | `docs/audit/raw/runtime/list_toolsets.json` | 73 toolsets `[VERIFIED-RUNTIME: 2026-07-30]` |
| Capture metadata | `docs/audit/raw/runtime/capture-metadata.json` | priority matrix list |
| Priority JSON schemas (12) | `docs/audit/raw/schemas/*.json` | see capture-metadata |

### Priority toolsets with runtime schemas

1. `editor_toolset.toolsets.blueprint.BlueprintTools` — 53 tools (source scan: 52; +1 runtime)
2. `NiagaraToolsets.NiagaraToolset_System` — 46 tools
3. `NiagaraToolsets.NiagaraToolset_Assets` — 3 tools
4. `GASToolsets.GameplayCueToolset` — 8 tools
5. `GASToolsets.AttributeSetToolset` — 2 tools
6. `GASToolsets.AbilitySystemInspectorToolset` — 4 tools
7. `GameplayTagsToolset.GameplayTagsToolset` — 6 tools
8. `editor_toolset.toolsets.material.MaterialTools` — 22 tools
9. `editor_toolset.toolsets.material_instance.MaterialInstanceTools` — 13 tools
10. `animation_toolset.toolsets.controlrig.ControlRigTools` — 44 tools (sample)
11. `editor_toolset.toolsets.programmatic.ProgrammaticToolset` — 2 tools
12. `UeremcpCore.UeremcpReferenceToolset` — 2 tools (`Ping`, `Echo`) **confirmed present**

## R-06 status recommendation

**Mitigate, do not close.**

R-06 is materially reduced for Wave 1 domains: disposition rows in `epic-toolsets.md`
now have registry-generated JSON Schema for the highest-duplication-risk Epic surfaces.
Agents can cite parameter shapes from `docs/audit/raw/schemas/` with
`[VERIFIED-RUNTIME: describe_toolset 2026-07-30]`.

R-06 remains **open** because:

1. **Coverage gap** — 73 toolsets load at runtime; only 12 have `describe_toolset` dumps
   (~16%). Remaining Epic plugins (PCG, UMG, SlateInspector, Sequencer*, Dataflow, etc.)
   and all 15 REAgentTools workflow toolsets lack schema files.
2. **REAgentTools schemas** — `list_toolsets` confirms all 15 RE toolsets register
   `[VERIFIED-RUNTIME: list_toolsets 2026-07-30]` but no `describe_toolset` dumps yet;
   `reagenttools.md` coexistence/cutover items still open.
3. **Naming map** — Runtime MCP names use Python module paths (`editor_toolset.toolsets.*`)
   while C++ plugins use short names (`EditorToolset.*`, `GASToolsets.*`). Dispositions
   must cite runtime names for schema lookup; source scan names remain valid for tool
   lists inside a class.
4. **Payload calibration** — No live `tools/call` sample payloads or token-size measurements
   for composite tools (`read_graph_dsl`, `get_connected_subgraph`, Niagara System ops).
5. **Async classification** — `execute_tool_script` exposes `outputSchema.returnValue: string`
   (JSON blob); per-tool async vs sync not exhaustively classified.
6. **Epic vs RE coexistence** — Both register simultaneously with no name collisions observed
   `[VERIFIED-RUNTIME: list_toolsets 2026-07-30]`; cutover bar still undefined (RB-15 q16).

## Runtime discoveries relevant to WS-01 / WS-03

| Finding | Implication |
|---|---|
| `UeremcpReferenceToolset` visible alongside Epic + RE | ADR-0002 host model works in shared registry |
| UObject refs serialize as `{refPath}` in schemas | Matches ADR-0004 pressure (not envelope graph JSON) |
| `GameplayTagsToolset.RenameTag` / `RemoveTag` lack `outputSchema` in dump | Agents get input-only guidance for destructive tag ops |
| `ProgrammaticToolset.execute_tool_script` input is single `script` string | R-04 coupling: FString path works; batching primitive confirmed at schema layer |

## Suggested next captures (WS-02 or WS-14)

When editor MCP is up, batch `describe_toolset` for:

- Remaining Niagara toolsets (`Component`, `Blueprint`, `Info`)
- `PCGToolset.PCGToolset`, `UMGToolSet.UMGToolSet`, `SlateInspectorToolset`
- All 15 `re_agent_tools.toolsets.*` RE workflow toolsets (especially `REBatchWorkflowTools`)
- One composite call per domain for payload size notes (read-only)

## Blockers

None for continuing Wave 1 workstreams. MCP was reachable via `user-unreal-mcp` and
REAgentTools `unreal_mcp_proxy` client on 2026-07-30.

## Response (WS-01)

**Date:** 2026-07-30  
**Decision:** Accept WS-02 recommendation — **mitigate R-06, do not close.**

Integrated `7443fda` on `ws-01-orch` (no-ff). Risk register and roadmap updated to
`runtime_partial`: 12 priority toolsets + full `list_toolsets` (73) captured on RE
2026-07-30. Residual gaps (remaining Epic toolsets, REAgentTools `describe_toolset`
dumps, payload calibration) stay on the R-06 open bar.

**Wave 2:** Still gated on Phase 1 exit (R-01, R-03, R-04, **R-06 closed**). R-04 is
closed; R-06 is materially reduced but not closed. No Wave 2 implementation started
from this integration.

**Further dumps:** WS-02 (or WS-14 opportunistically) may continue `describe_toolset`
captures when MCP is up; not a Phase 1 blocker for other workstreams.

