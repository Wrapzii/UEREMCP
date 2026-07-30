# WS-06: submit_graph patch mode — deferred proposal (post `8f3e0e3`)

- **From:** WS-06
- **Date:** 2026-07-30
- **Status:** Proposal only — **not implemented**

## Context

`8f3e0e3` landed scratch-only **`mode=replace`** for changed graphs:

- DSL via `extensions.blueprint.dsl` (preferred) or minimal JSON→DSL translator
- Epic `write_graph_dsl` + compile check + re-read semantic hash
- `/Game/__UeremcpTests/` path guard
- Honest statuses: `modified_and_validated` only when compile + re-read hash match

**Patch mode is intentionally not implemented in this slice.**

## Why patch is still needed (later)

| Gap | Replace-only limitation |
|---|---|
| Partial edits | Full graph replace via DSL deletes stale nodes; small pin/default tweaks still require full event bodies |
| Exotic nodes | MultiGate, Timeline, collapsed graphs fail DSL round-trip (`fidelity.lossy_areas`) |
| Variables / functions | Event-graph DSL does not express member variables or function graphs atomically |
| Agent cost | Replace forces re-serializing entire graph JSON even for one link change |

POC acceptance allows A8 to fail on complex graphs if **patch + A10** still meet A4–A7
(`docs/POC_ACCEPTANCE.md`). Replace-first was the correct P2 order.

## Proposed patch shape (specification extension — unchanged intent)

Extend `specification` only (ADR-0003). Draft ops (see also
`docs/proposals/ws-06-patch-mode-and-impl-plan.md`):

```json
{
  "base_revision": "<content_hash>",
  "ops": [
    { "op": "upsert_node", "semantic_id": "…", "type_id": "…", "defaults": {} },
    { "op": "remove_node", "semantic_id": "…" },
    { "op": "set_link", "from_semantic_id": "…", "from_pin": "then", "to_semantic_id": "…", "to_pin": "execute" },
    { "op": "break_link", "from_semantic_id": "…", "from_pin": "then", "to_semantic_id": "…", "to_pin": "execute" }
  ]
}
```

### Execution plan (when implemented)

1. Same gates as replace: scratch path (initially), `expected_revision`, dry_run default for first probe
2. Resolve `semantic_id` → live `UEdGraphNode` (create if `upsert_node` + known `type_id`)
3. Batch ops inside one editor transaction; **one** compile at end
4. Re-read + semantic hash compare on affected subgraph or full graph
5. Status ladder identical to replace (`partially_completed` vs `modified_and_validated`)

### Composability with replace

- Agent submits **full graph replace** when DSL is available and graph is small
- Agent submits **patch** when changing one default, one link, or when replace fidelity is lossy
- Toolset may internally translate small patches → localized DSL snippets via Epic primitives

## Explicit non-goals for first patch slice

- Production asset paths (remain scratch-only until WS-12 tier policy)
- Pin-id-based ops (semantic_id only)
- Auto-patch from arbitrary graph JSON diff (agent must emit ops)
- Claiming A6 editor-green without WS-11 harness proof

## Handoff

| Owner | Next step |
|---|---|
| **WS-06** | Schema draft in `schemas/domains/blueprints/submit_graph.schema.json` (`mode=patch` branch) after WS-01 schema window |
| **WS-11** | Editor tests: patch dry_run + one upsert_node on scratch BP |
| **WS-01** | Review patch op vocabulary vs ADR-0004 graph schema |

## Honest POC impact

- **A5 patch mode:** Not claimed
- **A6 write+verify:** Replace path exists in code; patch would add incremental verify — neither editor-green today
