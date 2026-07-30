# WS-06 → WS-11: aggregate Blueprint CompleteRoundTrip MCP proof

**Status:** Proposed  
**From:** WS-06  
**To:** WS-11  
**Baseline:** orchestrator tip `279f09a` plus the WS-06 commit carrying this proposal

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

