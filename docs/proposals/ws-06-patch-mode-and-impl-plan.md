# WS-06: Patch mode + implementation plan (post R-04)

- **From:** WS-06
- **Date:** 2026-07-29
- **Condition:** Do **not** implement `Plugins/UEREMCP/Source/UeremcpBlueprint/**` or
  `schemas/domains/blueprints/**` until Phase 1 host gate **R-04** closes
  (`docs/RISK_REGISTER.md`).

## Blockers (current)

| ID | Blocker | Owner |
|---|---|---|
| R-04 | `AICallable` JSON-string schema / host tool shape | WS-03 |
| Wave 1 | `UeremcpCore` toolset base + main-thread dispatch | WS-03 |
| Wave 1 | Envelope parse/validate + hashing helpers | WS-05 |
| Wave 1 | Editor test harness | WS-11 |
| Runtime | RE project editor session for `BP_RECharacter` ceiling | Ops / local |

Research (this run) is **not** blocked.

## Response (WS-01)

**Accepted as the post–R-04 implementation plan.** Do not start
`UeremcpBlueprint` / domain schemas until Phase 1 exit. Patch document shape is
a `specification` extension only. Note: Wave 1 host/protocol/harness pieces are
largely landed — remaining hard gate for Blueprint code is still **R-04** MCP
discoverability on RE (plus honest residual fidelity work).


## Architecture (accepted ADRs — no redesign)

- Host: in-process plugin toolset (ADR-0002)
- Envelope in/out (ADR-0003); extend **`specification` only**
- Graphs: `schemas/graph/graph.schema.json` (ADR-0004)
- Idempotency / `expected_revision` (ADR-0006)
- Compose Epic `BlueprintTools` + `execute_tool_script`; do not rebuild primitives

## Agent-facing actions (planned, post R-04)

| Action | Mode | Behavior |
|---|---|---|
| `blueprints.read_graph` | — | One call → graph JSON + diagnostics + fidelity; `response_detail` gates size |
| `blueprints.submit_graph` | `replace` | Full graph write via DSL and/or primitives; compile; re-read; hash |
| `blueprints.submit_graph` | `patch` | Semantic diff (below) when replace fidelity insufficient |
| `blueprints.submit_graph` | `create` / `create_or_update` | Asset+graph bootstrap |

Schemas live under `schemas/domains/blueprints/` (WS-06 owned) **after** R-04.

## Patch mode (if A8 fails on complex graphs)

Per POC_ACCEPTANCE: A8 may fail on complex graphs if A10 + patch still meet A4–A7.

### Patch document shape (specification extension — draft)

```json
{
  "base_revision": "<content_hash>",
  "ops": [
    { "op": "upsert_node", "semantic_id": "…", "type_id": "…", "pin_defaults": {}, "properties": {} },
    { "op": "remove_node", "semantic_id": "…" },
    { "op": "set_link", "from": "…", "from_pin": "then", "to": "…", "to_pin": "execute" },
    { "op": "break_link", "from": "…", "from_pin": "then", "to": "…", "to_pin": "execute" },
    { "op": "set_variable", "name": "Health", "type": {}, "default_value": 100, "is_replicated": true },
    { "op": "ensure_entry", "entry_kind": "event", "name": "EventBeginPlay" }
  ]
}
```

### Execution

1. Reject if `expected_revision` mismatch (ADR-0006).
2. Resolve `semantic_id` → live nodes (or create via `type_id`).
3. Apply ops with Epic primitives / small DSL snippets per entry — **one** compile at end.
4. Re-read complete graph (or affected subgraphs) + diagnostics.
5. Return `modified_and_validated` only if verification passes.

This still removes pin-micromanagement: agent submits a **batch of semantic ops**, not
N MCP round-trips.

## Implementation phases (after R-04)

### Phase P0 — module skeleton (1–2 days)

1. `UeremcpBlueprint` module + `UUeremcpBlueprintToolset` with one envelope echo tool
   proving R-04 shape.
2. Domain schemas: `read_graph` / `submit_graph` specification stubs.
3. Ownership check + schema validate green.

### Phase P1 — Read path (POC A1–A3)

1. C++ (preferred) walk: graphs → nodes → pins with **structured** `FEdGraphPinType`.
2. Map to `graph.schema.json`; compute `semantic_id` + `content_hash` (WS-05 helper).
3. Diagnostics: compiler messages + dead/disconnected walk.
4. Tests: scratch BP under `/Game/__UeremcpPoc/` (acceptance path) or
   `/Game/__UeremcpTests/`.
5. Optional: attach `extensions.blueprint.dsl` from `read_graph_dsl` for debug.

### Phase P2 — Replace path (POC A4–A11)

1. Translator: graph JSON → DSL for supported nodes; primitives for gaps.
2. `write_graph_dsl` or batched create/connect; single `compile_blueprint`.
3. Re-read; assert structure; `content_hash` identity on no-op replace (A8, A11).
4. Populate `fidelity.lossy_areas` honestly (RB-05 list).

### Phase P3 — Hardening

1. Patch mode for MultiGate / Timeline / MathExpression islands (or T3D internal).
2. `BP_RECharacter` (or largest RE BP) fidelity report with metrics.
3. Paging proposal if complete payload exceeds context (do not silently truncate).

## Success metrics (mandatory on responses)

- `metrics.mcp_round_trips` ≤ 3 for POC A scenario
- `metrics.internal_operations` = Epic tool calls inside
- Wall-clock + token estimate vs ~5:1 REAgentTools baseline (`docs/WHY.md`)

## Explicit non-goals until research revisit

- Replacing Epic BlueprintTools
- Agent-facing T3D
- Forking `graph.schema.json`
- Niagara / Material graph work (other WS)
