# WS-06 → WS-11: aggregate Blueprint CompleteRoundTrip MCP proof

**Status:** Proposed  
**From:** WS-06  
**To:** WS-11  
**Baseline:** orchestrator tip `886d09d` plus the WS-06 commit carrying this update

## Exact public MCP names

Call the UEREMCP semantic tool, not Epic `BlueprintTools` directly:

- `toolset_name`: `UeremcpBlueprint.UeremcpBlueprintToolset`
- `tool_name`: `SubmitGraph`
- argument key: `requestJson`
- request envelope: `action: "submit_graph"`, `mode: "replace"`

For the initial read, use the same toolset with `tool_name: ReadGraph`,
`requestJson`, and envelope `action: "read_graph"`.

[VERIFIED-RUNTIME: `user-unreal-mcp list_toolsets`, 2026-07-30; WS-11 transport
evidence `tests/integration/_logs/poc_a_complete_round_trip_70cc348.json`]

The A5 changed-replace path is Python-free. It constructs the currently supported
`EventBeginPlay → Branch → PrintString` semantic DSL slice with native K2 nodes,
connects pins with the graph schema, compiles with
`FKismetEditorUtilities::CompileBlueprint`, saves, and re-reads. It does not load
`PythonScriptPlugin` and does not call Epic `BlueprintTools`.
[VERIFIED: `EdGraphSchema.h:828`]
[VERIFIED: `KismetEditorUtilities.h:169`]
[VERIFIED: `K2Node_IfThenElse.h:22-44`]

This is deliberately a bounded native writer, not a claim that every Epic
Blueprint DSL expression is implemented. Unsupported changed-write DSL is rejected
before graph mutation with the supported shape in the error.

## What WS-06 now exposes

`read_graph` always returns the complete-state context arrays, including empty
`variables` and `dependencies`, instead of omitting them. A changed `submit_graph`
replace returns its internally re-read complete graph under `diagnostics.graphs` and
reports `validation.compiled` plus `validation.saved`. An unchanged replace returns
the complete current graph under the same diagnostics shape. Both complete graphs
retain their `fidelity` object and `content_hash`.

This is intended to let a transport-level harness collect the A1/A2/A5 evidence
without issuing an extra inspect call after the changed replace.

## Requested WS-11 filter

Add a `CompleteRoundTrip` editor/integration filter that invokes the registered
Blueprint tools through the MCP transport, not by calling
`UUeremcpBlueprintToolset` C++ methods directly:

1. One `read_graph` MCP call: assert A1 and the A2 complete payload fields
   (`nodes`, pins, pin types, defaults, `links`, `variables`, `entry_points`,
   `dependencies`) plus explicit `fidelity`.
2. One changed `submit_graph` MCP call with a complete graph payload and
   `mode: replace`: assert A5 `modified_and_validated`,
   `validation.compiled: true`, `validation.saved: true`,
   `validation.reread_after_write: true`, and assert the returned
   `diagnostics.graphs[0]` carries the complete re-read graph.
3. One unchanged `submit_graph` MCP call using the returned complete graph:
   assert `no_change_required`, hash identity, and a complete graph plus fidelity
   in `diagnostics.graphs[0]`.

Record the actual MCP call count and raw response evidence. Reuse
`/Game/__UeremcpPoc/` assets and keep the RE/VisualTest plugin junction on the
orchestrator plugin.

## Non-claims

The WS-06 editor automation test remains an in-process implementation proof. It is
not transport-level MCP evidence. This proposal does not claim A1, A2, A5, A9, A10,
or overall POC A PASS; WS-11 must run and record the aggregate transport filter.

