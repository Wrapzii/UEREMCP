# RB-05: Blueprint graph serialisation and reconstruction fidelity

- **Owner:** WS-06
- **Status:** in_progress (research ceiling established; implementation blocked on R-04)
- **Blocks:** POC A, ADR-0004 confidence, project's central promise
- **Priority:** highest
- **Last updated:** 2026-07-29
- **Worktree:** `$UEREMCP_ROOT-ws06` (`ws-06-blueprint`)

## Executive verdict

**Whole-graph read is tractable today** via Epic `BlueprintTools` (structured
`NodeInfo` / pin walks) and/or `read_graph_dsl`. **Whole-graph write is tractable for
a large, well-defined subset** via `write_graph_dsl` (and the same primitives the DSL
already uses). **Epic DSL is not isomorphic to `graph.schema.json`** — it is a usable
*internal* write backend and a *lossy* logic view, not the agent contract.

POC A should **compose over Epic**, not rebuild node primitives. The UEREMCP gap is
envelope + stable IDs/hashes/diagnostics/verification (`ADR-0003/0004/0006`), not
"can we create a PrintString node."

Runtime editor this session was the **visualtest** project (proxy
`state_dir`…`visualtest`), not RE. Scratch assets only under
`/Game/__UeremcpTests/`. `BP_RECharacter` (RE, ~1.9 MB on disk) was **not** loaded —
read-only size recorded from filesystem only.

---

## Questions (answered)

### A. Reading

#### 1. Graph enumeration

`UBlueprint` editor-only arrays
`UbergraphPages`, `FunctionGraphs`, `DelegateSignatureGraphs`, `MacroGraphs`,
`EventGraphs` exist
`[VERIFIED: Engine/Source/Runtime/Engine/Classes/Engine/Blueprint.h:543-563]`.

Out-of-tree / Python agent path that Epic already exposes:

- `BlueprintEditorLibrary.list_graphs(blueprint)` → all graphs
  `[VERIFIED: BlueprintEditorLibrary.h:180]`
- `BlueprintEditorLibrary.find_event_graph(blueprint)`
  `[VERIFIED: BlueprintEditorLibrary.h:111]`
- `BlueprintTools.list_graphs` / `get_graph`
  `[VERIFIED: blueprint.py:233-261]`
  `[VERIFIED-RUNTIME: list_graphs on /Game/__UeremcpTests/BP_Ws06RoundTrip → EventGraph + UserConstructionScript]`

#### 2. Generic `UEdGraphNode` properties

Public fields: `Pins`, `NodePosX/Y`, `NodeComment`, `NodeGuid`, `ErrorType`,
`ErrorMsg`, `AllocateDefaultPins`, `ReconstructNode`, `GetNodeTitle`
`[VERIFIED: EdGraphNode.h:293-406,704-745]`.

Epic agent surface aggregates via `_get_node_info` → `NodeInfo{type_id, node, position,
input_pins, output_pins}` where `type_id = Category|Title` (custom events prefixed
`AddEvent|Custom|…`)
`[VERIFIED: blueprint.py:18-23,609-628]`
`[VERIFIED-RUNTIME: get_node_infos samples on scratch BP]`.

`FProperty` reflection alone is **insufficient** for call targets / member refs —
`UK2Node_CallFunction` stores `FMemberReference FunctionReference` and
`SetFromFunction` / `GetTargetFunction`
`[VERIFIED: K2Node_CallFunction.h:71,150,209]`. Generic property dump misses that
semantics; need typed readers or Epic `type_id` + pin values.

#### 3. `UEdGraphPin` / `FEdGraphPinType`

Full type struct: `PinCategory`, `PinSubCategory`, `PinSubCategoryObject`,
`ContainerType`, `bIsReference`, `bIsConst`, plus `DefaultValue`, `DefaultObject`,
`LinkedTo`, `PinId`, `PinName`
`[VERIFIED: EdGraphPin.h:76-115,303-398]`.

**Gap:** Epic `PinInfo.type_id` is
`str(pin.get_pin_type_display_string())` (e.g. `"Float (double-precision)"`,
`"Actor Object Reference"`, `"Linear Color Structure"`), **not** the structured
`FEdGraphPinType` fields required by `graph.schema.json` `pin_type`
`[VERIFIED: blueprint.py:622]`
`[VERIFIED-RUNTIME: PrintString pin dump]`.

Implication: UEREMCP read path for ADR-0004 fidelity should use **C++** (or a richer
Python pin API if exposed) to emit `category` / `sub_category` /
`sub_category_object` / `container`. Mapping display strings → schema is lossy.

#### 4. Variables / locals / params / replication

`FBPVariableDescription` carries `VarName`, `VarGuid`, `VarType`, `PropertyFlags`,
`RepNotifyFunc`, `ReplicationCondition`, `DefaultValue`, metadata
`[VERIFIED: Blueprint.h:201-241]`.
Member list: `UBlueprint::NewVariables`
`[VERIFIED: Blueprint.h:592]`.

Epic tools: `list_variables`, `add_variable` (+ struct/object variants), function
param add/remove on graphs
`[VERIFIED: blueprint.py tool surface; WS-02 q8]`.

#### 5–6. Diagnostics and compile → node mapping

Compiler path:

- `UBlueprintEditorLibrary::CompileBlueprint` → bool
  `[VERIFIED: BlueprintEditorLibrary.h:200]`
- Shared helper polls `blueprint.Status` (`BS_UP_TO_DATE` /
  `BS_UP_TO_DATE_WITH_WARNINGS`) then
  `BlueprintGraphEditor.list_nodes_with_errors()` + `ErrorMsg`
  `[VERIFIED: toolset_registry/helpers.py:30-44]`
  `[VERIFIED-RUNTIME: compile_blueprint on scratch BP succeeded]`
- Lower-level: `FKismetEditorUtilities::CompileBlueprint(..., FCompilerResultsLog*)`
  `[VERIFIED: KismetEditorUtilities.h:169]`
- `FCompilerResultsLog::FindSourceObject` maps intermediate objects back to sources
  `[VERIFIED: CompilerResultsLog.h:61-62]`
- Per-node: `UEdGraphNode::ErrorMsg` / `ErrorType`
  `[VERIFIED: EdGraphNode.h:397-402]`

**Must derive by traversal** (Epic DSL does not emit these): dead nodes,
disconnected subgraphs, unused outputs. Compiler covers type/link errors that
manifest as `ErrorMsg`. POC A `diagnostics` = compiler messages ∩ graph walk.

Compile is **synchronously awaitable** at the BlueprintTools layer (call returns
after status check). No separate async job API required for single-asset compile
`[VERIFIED: helpers.py:30-44]`.

---

### B. Writing

#### 7–9. Generic construction, CallFunction, connections

Epic does **not** require per-`K2Node_*` C++ construction for the common case:

- `BlueprintGraphEditor.create_node_from_name(type_id, pos, …)`
- macro path via `add_macro_node`
- custom events / dispatcher helpers
  `[VERIFIED: blueprint.py:877-915]`

Spawner substrate: `FBlueprintActionDatabase::Get()` / `GetAllActions()`
`[VERIFIED: BlueprintActionDatabase.h:46-94]`.

Connections: `pin.try_create_connection` (schema validation) used by
`BlueprintTools.connect_pins`
`[VERIFIED: blueprint.py:1058-1070]`
and schema overrides `UEdGraphSchema_K2::CanCreateConnection` /
`TryCreateConnection`
`[VERIFIED: EdGraphSchema_K2.h:568-569]`.

`write_graph_dsl` is a **Transpiler** over `create_node` + `connect_pins` +
`set_pin_value` + `delete_node`, then `compile_blueprint`
`[VERIFIED: blueprint.py:1454-1477]`
`[VERIFIED: blueprint_dsl.py:835-1718]`.

#### 10. Node types — fidelity table

| Class / family | Reconstruct? | Evidence |
|---|---|---|
| `K2Node_CallFunction` / PrintString / math ops | Clean via DSL or `create_node` | `[VERIFIED-RUNTIME: round-trip]` |
| Branch / if / else | Clean (DSL sugar) | `[VERIFIED-RUNTIME]` + unit tests in engine |
| ForLoop / ForEach / While (macros) | Clean; `create_node` resolves macro path | `[VERIFIED-RUNTIME: ForLoop create]` `[VERIFIED: blueprint.py:908-910]` |
| Switch on Int | Clean; decompiler expands alias | `[VERIFIED-RUNTIME: identical rewrite]` |
| Latent Delay (single `then`) | Clean; decompile flattens `(:then …)` to sequential stmts | `[VERIFIED-RUNTIME]` |
| Multi-exec latent / Ability tasks | Supported in DSL grammar (`(:ExecOut …)`) | `[VERIFIED: blueprint_dsl.py:53-96]` — Ability WaitDelay not exercised this run |
| `MultiGate` | **Write ok; decompile hard-fails if reachable** | `[VERIFIED: blueprint_dsl.py:2199-2201]` `[VERIFIED-RUNTIME: write error path / prior orphan]` |
| Sequence | Creates (`then_0`, `then_1`); DSL has `_emit_sequence` | `[VERIFIED-RUNTIME: create]` `[VERIFIED: blueprint_dsl.py:2203-2205]` |
| Timeline | Special type `|AddTimeline...` → node type_id `\|Timeline` | `[VERIFIED-RUNTIME]` — **not** in DSL USAGE examples |
| Custom event | `AddEvent|Custom|Name` | `[VERIFIED-RUNTIME]` |
| Reroute / Knot | Decompiler follows chains; not emitted as nodes | `[VERIFIED: blueprint_dsl.py:631-643,2207-2212]` |
| CreateDelegate / bind-unbind | Dedicated BlueprintTools helpers exist | `[VERIFIED: blueprint.py:632-677]` — DSL coverage incomplete |
| `K2Node_MathExpression` | `find_node_types("Math Expression")` → `[]` this run | `[VERIFIED-RUNTIME]` — treat as **special / T3D** until proven |
| Macro instance (project macros) | Engine macros via cache; project macros likely same path | `[VERIFIED: blueprint.py:908-910]` — project macros `[UNVERIFIED]` beyond path |
| Collapsed / Composite / MathExpression subgraph | Headers exist (`K2Node_Composite`, `K2Node_MathExpression`) | `[VERIFIED: K2Node_Composite.h, K2Node_MathExpression.h]` — **no DSL round-trip proof** |
| Project custom K2 nodes | Only if registered in action database / `create_node_from_name` | Unknown until enumerated per project |

#### 11. T3D / clipboard as internal fidelity

`FEdGraphUtilities::ExportNodesToText` / `ImportNodesFromText` /
`CanImportNodesFromText` are public
`[VERIFIED: EdGraphUtilities.h:110-127]`.

ADR-0004 correctly rejects T3D as the **agent** contract. As an **internal** escape
hatch for Timeline / MathExpression / custom K2 / MultiGate islands, it is legitimate
and should be evaluated in Phase 2 behind `fidelity.lossy_areas` / a
`extensions.blueprint.t3d_fragments[]` proposal — **not** exposed raw to agents.

#### 12. Delete/recreate vs external references

Function **names** and asset path stability preserve cross-BP call-by-name when
function graphs are recreated with the same signatures
`[UNVERIFIED]` pending dedicated test.
Level actor event bindings and child-BP overrides keyed by **NodeGuid** will **not**
survive node rebuild
`[VERIFIED: EdGraphNode.h:404-406 NodeGuid is per-node identity]` + ADR-0004
delete-and-recreate policy.

`write_graph_dsl` `delete_stale` removes unreachable pre-existing nodes after
transpile
`[VERIFIED: blueprint_dsl.py:769-818]`.

---

### C. Stability and identity

#### 13. What survives a rebuild? → `semantic_id`

| Identity | Survives rebuild? | Notes |
|---|---|---|
| `UEdGraphNode::NodeGuid` | **No** (new nodes) | `[VERIFIED: EdGraphNode.h:404-406]` |
| Epic `PinID{node refPath, index_id, direction}` | **No** | `[VERIFIED: blueprint_node.py:8-12]` `[VERIFIED-RUNTIME]` |
| `type_id` (`Category\|Title`) | Role-stable if title stable | `[VERIFIED: blueprint.py:18-23]` |
| Function/event **name** | Yes if intentionally preserved | |
| DSL bind names | **Not stable** — decompiler elides binds | `[VERIFIED-RUNTIME: bind loc → inlined GetActorLocation]` |

**Proposed `semantic_id` derivation** (for WS-05 / templates):

```
semantic_id = "{graph_name}/{entry_kind}:{entry_name}/n{topo_index}:{type_id}[#{disambig}]"
```

Where:

- `entry_kind` ∈ `event` | `fn` | `macro`
- `topo_index` = deterministic order along exec+pure DAG from that entry
  (stable sort: type_id, then sorted pin default fingerprint, then child order)
- `disambig` = bound `UFunction` path / custom event name / timeline name when
  `type_id` collides

`node_id` remains retrieval-local (ADR-0004). Patches and templates reference
`semantic_id` only.

#### 14. `content_hash` (WS-05 blocker answer)

**Do not hash raw DSL text.** Decompiler non-determinism / bind elision /
case-label reorder can change text without semantic change
`[VERIFIED-RUNTIME: bind elision; switch case order Default/0/1]`.

**Hash this canonical structured payload** (UTF-8 JSON, sorted keys):

Include:

- `graph_type`, `graph_name`
- variables (name, type struct, default, replication fields) sorted by name
- nodes: `semantic_id`, `node_class`/`type_id`, `properties` (call targets, etc.),
  pin defaults that differ from autogenerated, enabled state
- links: `{from_semantic_id, from_pin_name, to_semantic_id, to_pin_name}` sorted
- comments text (optional policy: include — they signal intent per schema)

Exclude:

- `position`, `NodeGuid`, engine `refPath`, retrieval-local `node_id`/`pin_id`
- autogenerated default values that match schema autogen
- pure cosmetic reroute knots (follow-through already in links)

`revision` may equal `content_hash` for v1 (ADR-0006 open question) or a
monotonic overlay later.

#### 15. Compile nondeterminism

Generated bytecode / CDO CRCs (`CrcLastCompiledCDO`) exist on `UBlueprint`
`[VERIFIED: Blueprint.h:732-735]` but are **out of** content_hash scope.
Graph structured hash above should be stable across identical rebuilds **if**
semantic_id topo is deterministic. Confirm with POC A A8 on scratch + one medium BP
after R-04.

---

### D. Scale

#### 16–17. Payload / timing

| Asset | Source | Graphs | EventGraph nodes | DSL bytes | `get_node_infos` JSON | Time |
|---|---|---|---|---|---|---|
| `BP_Ws06RoundTrip` (scratch) | `[VERIFIED-RUNTIME]` | 2 | 10 (simple) | ~253 | ~14 KB | write+read ~0.5–4 s scripted |
| `BP_ThirdPersonCharacter` | `[VERIFIED-RUNTIME]` | 9 | 27 | 1208 | ~57 KB | ~2.0 s read path |
| `BP_RECharacter` | disk only | — | — | — | — | **uasset 1939 KB** at `Content/RE/Core/` — **not loaded** this session (editor was visualtest) |

**Calibration:** `response_detail: complete` is fine for typical ThirdPerson-scale
graphs. Character BPs the size of `BP_RECharacter` likely need either
`get_connected_subgraph` per entry (Epic already has this
`[VERIFIED: blueprint.py:722-748]`) or a future **paging** extension to ADR-0004 —
file as proposal, do not fork schema in this run.

---

## Epic DSL ↔ `graph.schema.json` bridge

```
Agent  ←→  graph.schema.json (ADR-0004)  ←→  UEREMCP Blueprint service
                                              │
                    ┌─────────────────────────┼─────────────────────────┐
                    ▼                         ▼                         ▼
            Structured read            DSL write backend          Primitive/T3D
         (NodeInfo / C++ pins)      (write_graph_dsl)            (exotic islands)
                    │                         │
                    └──────── diagnostics / compile / re-read ──────────┘
```

1. **Read (POC A A1–A3):** C++ (preferred) or batched Epic tools → fill
   `nodes`/`links`/`variables`/`diagnostics`/`fidelity`. Optionally also return
   `extensions.blueprint.dsl` for humans/debug — never as sole representation.
2. **Replace (A4–A8):** Prefer translating graph JSON → DSL for supported subset;
   fall back to primitive `create_node`/`connect_pins` for nodes DSL cannot express;
   T3D fragments only for classified exotic islands; always compile + re-read + hash.
3. **Do not** expose `read_graph_dsl`/`write_graph_dsl` as the agent-facing contract
   (no stable IDs, UObject pin refs, no envelope)
   `[VERIFIED: WS-02 docs/audit/epic-toolsets.md q8]`.

---

## Audit (before any new primitive)

Already exists and must be **preserved / composed**, not rebuilt
(`[VERIFIED: WS-02 epic-toolsets.md]` + this brief):

| Epic tool | Role |
|---|---|
| `read_graph_dsl` / `write_graph_dsl` / `get_graph_dsl_docs` | Logic round-trip + compile |
| `create_node`, `delete_node`, `connect_pins`, `break_pins`, pin value ops | Primitives DSL uses |
| `find_nodes`, `get_node_infos`, `get_connected_subgraph` | Structured inspection |
| `compile_blueprint` | Sync compile + error nodes |
| `list_graphs`, `get_graph`, variables/functions/events tools | Asset scope |
| `ProgrammaticToolset.execute_tool_script` | Batch many of the above in one MCP hop |

**UEREMCP-only gap (justifies new agent-facing actions, not new pin primitives):**

- ADR-0003 envelope + verified statuses
- `graph.schema.json` mapping + `semantic_id` / `content_hash` / `expected_revision`
- Mandatory diagnostics block
- Honest `fidelity.lossy_areas`
- Goal-level `blueprints.read_graph` / `blueprints.submit_graph`

Proposal for WS-02 matrix rows: `docs/proposals/ws-06-audit-blueprint-rows.md`.

---

## Negative findings

1. **No separate `BlueprintNodeTools`** — only `BlueprintTools` (52 tools). REAgentTools
   docs that name both are wrong
   `[VERIFIED: WS-02 q8]`.
2. **`PinInfo` lacks structured `FEdGraphPinType`** — display strings only
   `[VERIFIED: blueprint.py:622]`.
3. **`MultiGate` cannot be decompiled** when on an exec chain
   `[VERIFIED: blueprint_dsl.py:2199-2201]`.
4. **DSL text ≠ semantic identity** (bind elision, sugar expansion)
   `[VERIFIED-RUNTIME]`.
5. **`find_node_types` JSON schema requires `context_pins`** even though Python default
   is `[]` — callers must pass `"context_pins": []`
   `[VERIFIED-RUNTIME]`.
6. **`Math Expression` type filter returned empty**; Timeline uses `|AddTimeline...`
   not a normal Category|Title
   `[VERIFIED-RUNTIME]`.
7. **Orphan / unreachable nodes are invisible to `read_graph_dsl`** (exec-root walk)
   — MultiGate created off-chain did not break decompile earlier
   `[VERIFIED: decompiler entry walk blueprint_dsl.py:2470+]`.
8. **`BP_RECharacter` / RE complex round-trip not runtime-verified** this session
   (wrong project loaded). Disk size only.
9. **Host gate R-04 open** — no UEREMCP `AICallable` envelope implementation this run
   (`docs/RISK_REGISTER.md`).
10. Expected **async compile future** separate from tool return — **not required** for
    BlueprintTools path; compile completes inside the tool call
    `[VERIFIED: helpers.py]`.

---

## `fidelity.lossy_areas` (ready for responses)

- `pin_type_as_display_string` (until C++ structured pin types land)
- `node_guid_not_preserved`
- `dsl_bind_elision`
- `reroute_knots_elided`
- `multigate_no_dsl_roundtrip`
- `timeline_special_spawn`
- `math_expression_unproven`
- `collapsed_composite_unproven`
- `custom_k2_project_nodes_unproven`
- `positions_not_semantic` (excluded from content_hash by design)

---

## Deliverables checklist

- [x] Simple DSL retrieve → replace unchanged → retrieve identical
      `[VERIFIED-RUNTIME: BP_Ws06RoundTrip]`
- [ ] `content_hash` identical under UEREMCP hasher — **blocked on WS-05 implementing
      hash over structured canonical form (q14)**; DSL-identical ≠ hash contract
- [~] Complex BP: ThirdPerson EventGraph read OK (27 nodes); RE `BP_RECharacter`
      **not** round-tripped (editor project mismatch)
- [x] Node type classification table (above)
- [x] `fidelity.lossy_areas` list
- [x] Answers to 13 and 14 for WS-05
- [x] Patch-mode design if replace incomplete — see proposal
  `docs/proposals/ws-06-patch-mode-and-impl-plan.md`

---

## Runtime evidence log

| Probe | Result |
|---|---|
| Create `/Game/__UeremcpTests/BP_Ws06RoundTrip` | OK |
| `write` simple BeginPlay + if/PrintString; `read`; `write(read)`; `read` | `roundtrip_identical: true` |
| Delay latent + Switch int same pattern | `identical: true` |
| MultiGate decompile | Hard error string from Epic |
| Timeline create via `\|AddTimeline...` | OK → type_id `\|Timeline` |
| ThirdPerson EventGraph | 27 nodes, 1208 B DSL, 56872 B infos, 1952 ms |
| Editor MCP | Intermittent WinError 10061 / lock timeout; recovered |

Scratch only; no user gameplay assets modified intentionally. ThirdPerson was
**read-only**.
