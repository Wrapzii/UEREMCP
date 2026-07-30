# WS-06 → WS-02: Blueprint audit matrix rows

- **From:** WS-06
- **To:** WS-02 (`docs/audit/**`)
- **Date:** 2026-07-29
- **Status:** proposal (do not edit `docs/audit/` from WS-06)

## Purpose

Supply Blueprint-domain dispositions for the Epic capability matrix, grounded in
RB-05 + runtime probes. WS-02 owns the file; please merge or reject.

## Suggested rows / updates

| Toolset / tools | Altitude | Disposition | Superseded by (UEREMCP) | Limitations | Tag |
|---|---|---|---|---|---|
| `BlueprintTools` (52) | primitive + DSL composite | **preserve** primitives; **supersede** agent surface | `blueprints.read_graph`, `blueprints.submit_graph` | UObject pin refs; DSL ≠ `graph.schema.json`; no envelope/hash/revision | `[VERIFIED: blueprint.py]` + RB-05 |
| `read_graph_dsl` / `write_graph_dsl` | composite | **preserve** as *internal* backend | composed under submit/read | MultiGate decompile fail; bind elision; no semantic_id | `[VERIFIED: blueprint.py:1454-1502]` `[VERIFIED-RUNTIME: RB-05]` |
| `get_node_infos` / `find_nodes` / `get_connected_subgraph` | composite inspect | **preserve** / internalise | feed structured read | `PinInfo.type_id` is display string, not `FEdGraphPinType` | `[VERIFIED: blueprint.py:609-748]` |
| `compile_blueprint` | primitive | **preserve** | validation pipeline | Sync; errors via `ErrorMsg` / `list_nodes_with_errors` | `[VERIFIED: helpers.py:30-44]` |
| `ProgrammaticToolset.execute_tool_script` | batch | **preserve** | compose under `execute_plan` | Required for multi-tool Blueprint jobs without MCP thrash | WS-02 q7 |

## Correction to propagate

REAgentTools / older docs naming **`BlueprintNodeTools`**: class **does not exist** in
UE 5.8 EditorToolset. Only `BlueprintTools`
`[VERIFIED: WS-02 q8]`.

## Do-not-rebuild (WS-06 commitment)

WS-06 will **not** reimplement create/connect/compile pin primitives. New code is
envelope mapping, hashing, diagnostics, and verification only.

## Response (WS-01)

**Closed — folded by WS-02** (`b15ee88`, orch `e61293e`) into
`docs/audit/epic-toolsets.md`. Preserve Epic `BlueprintTools` as internal
backend; agent surface is goal-level read/submit.
