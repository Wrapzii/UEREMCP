# RB-02: Actual tool inventory of Epic's 27 toolsets

- **Owner:** WS-02
- **Status:** complete (source-verified; runtime enumeration blocked — see `docs/audit/raw/runtime-negative-findings.json`)
- **Blocks:** every domain workstream
- **Priority:** highest — start immediately

## Why this is the highest-leverage first task

Epic ships 27 domain toolsets in `$TS` `[VERIFIED: directory listing]`, covering
Niagara, GAS, gameplay tags, PCG, StateTree, UMG, physics, animation, data registry,
game features, automation testing, semantic search, and more.

**Directory names are confirmed. Tool names are not.** Everything currently "known"
about specific Epic tools (`NiagaraToolsets.*`, `BlueprintTools`, `BlueprintNodeTools`,
`MaterialTools`, `ObjectTools.get_properties`, `ActorTools.set_actor_transform`,
`LogsToolset.GetLogEntries`, `ProgrammaticToolset.execute_tool_script`) comes from
REAgentTools' own documentation — second-hand, and tagged `[UNVERIFIED]` in
`GROUNDED_FACTS.md §7.5`.

Until this brief lands, every domain workstream is at risk of rebuilding a tool that
already works. That is the single most likely way this project wastes its effort.

## Questions

1. **Which toolsets actually load** in the RE project at runtime? `RE.uproject` enables
   only some by name, but `AllToolsets` may pull in others
   `[VERIFIED: RE.uproject plugin list]`. Enumerate what is really registered, not what
   is configured.
2. **For each loaded toolset:** exact toolset name, version, description, and the full
   list of tool names with their JSON Schemas.
3. Where does each tool sit on the primitive↔composite spectrum? A tool that already
   does something goal-level is a tool we must not duplicate.
4. What do result payloads actually look like — compact, or large object dumps? This
   determines how much filtering our composition layer must do.
5. Which tools are **async**, and which block the editor?
6. Which tools are **destructive** and what safety they have.
7. Does `ProgrammaticToolset.execute_tool_script` exist, and what exactly does it
   accept? REAgentTools built its Niagara batching strategy on it
   `[UNVERIFIED — from $RAT/Docs/NIAGARA_BATCHING.md]`. It may already be the batching
   primitive `execute_plan` should compose. **High value — check early.**
8. What Blueprint graph/node tools exist, and how far do they get? If Epic already has
   working node-level authoring, WS-06's job becomes *composition over* them rather
   than building graph writes from scratch. This materially changes RB-05's scope.
9. What do `GASToolsets`, `NiagaraToolsets`, `GameplayTagsToolset`, `PCGToolset`,
   `StateTreeToolset`, `UMGToolSet`, `PhysicsToolsets` cover?
10. What is `SemanticSearchToolset`? If it provides project-wide semantic asset search,
    it is directly relevant to template similarity matching (WS-15).
11. What is `MCPClientToolset` — does it let the editor act as an MCP *client*, e.g.
    calling out to the Blender MCP server the project already configures
    `[VERIFIED: $PROJ/.mcp.json]`? Hand off to RB-11.
12. What is `DataflowAgent`, and `ConversationToolset`?
13. What is `AutomationTestToolset` — hand off to RB-14, it may be our test harness.

## Method

Prefer runtime enumeration over reading source; it answers "what actually loads" at
the same time.

1. From the editor Python console, enumerate registered toolsets via the
   `toolset_registry` Python API (`_registry_interface.py`, `helpers.py`).
2. Or call `list_toolsets` / `describe_toolset` from a connected MCP client — with
   `bEnableToolSearch = true` this is the intended discovery path
   `[VERIFIED: ModelContextProtocolSettings.h]`.
3. Or in C++, `FToolsetRegistry::ForEachToolset` plus `GetToolsetJsonSchemas()`
   `[VERIFIED: ToolsetRegistry.h]`.
4. Cross-check against `$TS/<Toolset>/Source/` for `meta=(AICallable)` UFUNCTIONs to
   catch anything filtered out at runtime.

Dump raw output to `docs/audit/raw/` so others can grep it without repeating the work.

## Deliverables

- [x] `docs/audit/epic-toolsets.md` — capability matrix + dispositions (source-verified; runtime schemas pending)
- [x] `docs/audit/raw/` — source scan dumps, q7/q8 JSON, runtime negative findings, per-plugin JSON
- [x] Do-not-rebuild list (in epic-toolsets.md)
- [x] Real gaps list (in epic-toolsets.md)
- [x] q7 and q8 answered for WS-05/WS-06 (see `docs/audit/raw/q7-*.json`, `q8-*.json`)
- [ ] Runtime `describe_toolset` JSON Schema dumps — **blocked: editor MCP offline**
