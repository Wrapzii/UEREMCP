# RB-05: Blueprint graph serialisation and reconstruction fidelity

- **Owner:** WS-06
- **Status:** not_started
- **Blocks:** POC A, ADR-0004 confidence, the project's central promise
- **Priority:** highest — start immediately

## Why this decides the project

`docs/WHY.md`: the owner's stated pain is "micromanaging nodes on characters."
Removing it requires that a Blueprint graph can be **read** as complete structured
JSON and **written back** from it.

Read is very likely tractable. **Write is the open question**, and the answer
determines whether this project delivers transformation or merely good inspection.

Either answer is a success for this brief. A clear, evidenced "whole-graph
reconstruction is not achievable for X and Y; here is what is" in week one is worth
more than an optimistic assumption discovered in week six.

## Questions

### A. Reading

1. What public/editor API enumerates a `UBlueprint`'s graphs — `UbergraphPages`,
   `FunctionGraphs`, `MacroGraphs`, `DelegateSignatureGraphs`,
   `EventGraphs`? Which are reachable from an out-of-tree plugin?
2. For a `UEdGraphNode`: how do you get class, title, position, pins, and per-node
   properties generically (rather than per-node-type)? Is `FProperty` reflection over
   the node object sufficient?
3. For a `UEdGraphPin`: how do you read `PinType` fully — `PinCategory`,
   `PinSubCategory`, `PinSubCategoryObject`, `ContainerType`, `bIsReference`,
   `bIsConst` — plus `DefaultValue`, `DefaultObject`, `DefaultTextValue`, and
   `LinkedTo`?
4. How are variables, local variables, function parameters, and their replication
   settings read?
5. How do you compute the `diagnostics` block of `graph.schema.json` — dead nodes,
   disconnected subgraphs, invalid links, missing required inputs, type mismatches?
   Which does the Blueprint compiler already report, and which must we derive by graph
   traversal?
6. Where do compiler results live after a compile, and how do you map a message back
   to a specific node/pin so `diagnostic.node_id` can be populated?

### B. Writing — the hard part

7. Can nodes be created generically from a class name plus properties, or does each
   `K2Node_*` type need bespoke construction? Investigate
   `FEdGraphSchemaAction`/`UEdGraphSchema_K2` node-spawner paths and
   `FBlueprintActionDatabase`.
8. What is the correct way to recreate a `K2Node_CallFunction` bound to a specific
   `UFunction`, including pin reconstruction via `AllocateDefaultPins` /
   `ReconstructNode`?
9. Can pins be connected programmatically with full validation
   (`UEdGraphSchema::TryCreateConnection`, `CanCreateConnection`), and do wildcard
   and container types resolve correctly?
10. **Which node types resist reconstruction?** Enumerate honestly and specifically.
    Suspects: latent/async nodes, timelines, delegate bind/unbind, custom events with
    parameters, macro instances, collapsed graphs, `K2Node_Knot` (reroute), Blueprint
    interfaces, `K2Node_MathExpression`, spawn-actor-from-class with its dynamic pins,
    and any project-authored custom K2 nodes.
11. Is `T3D` / clipboard text (`FEdGraphUtilities::ExportNodesToText` /
    `ImportNodesFromText`) usable as an **internal** fidelity mechanism? ADR-0004
    rejects it as the *agent contract*, not as an implementation detail. If it
    round-trips exotic nodes that generic construction cannot, a hybrid is legitimate
    — evaluate seriously.
12. Does deleting and recreating an entire graph preserve external references to it —
    other Blueprints calling its functions, level actors bound to its events, child
    Blueprints?

### C. Stability and identity

13. What survives a rebuild? `FGuid NodeGuid` presumably does not. Confirm, and design
    `semantic_id` (ADR-0004) accordingly — what deterministic derivation is stable
    across rebuilds and unique within a graph?
14. What should `content_hash` be computed over so it ignores node positions and GUIDs
    but catches a changed pin default or a moved connection? WS-05 needs this answer.
15. Does compiling produce nondeterministic output that would make the hash unstable
    across identical rebuilds?

### D. Scale

16. What is the node count of the largest Blueprint in the RE project, and how large is
    its complete JSON representation? This calibrates whether `response_detail:
    complete` is usable in practice or whether graph *paging* is required — a real
    possibility ADR-0004 does not currently provide for.
17. How long does retrieve → replace → compile → re-retrieve take on that Blueprint?

## Negative findings

Record every API you expected to exist and did not, every private-only path, every
node type that resisted reconstruction. This section is the most valuable part of the
brief.

## Deliverables

- [ ] Round-trip proof on a **simple** graph: retrieve → replace unchanged → retrieve,
      `content_hash` identical — `[VERIFIED-RUNTIME]`
- [ ] Round-trip attempt on `BP_PlayerCharacter` or the project's most complex
      Blueprint, with an honest fidelity report
- [ ] A table of node types classified: reconstructs cleanly / needs special handling /
      cannot reconstruct
- [ ] A `fidelity.lossy_areas` list ready to populate in real responses
- [ ] Answers to 13 and 14, which WS-05 is blocked on
- [ ] If whole-graph replace is not viable, a concrete `patch`-mode design that still
      removes the node-micromanagement loop
