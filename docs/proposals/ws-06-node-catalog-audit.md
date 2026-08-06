# WS-06 — `describe_node_catalog` capability audit

**Status:** implemented on `ws-06-07-10-tool-expansion`.
**Owner of the destination file:** WS-02 (`docs/audit/epic-toolsets.md`). This document
is the AGENTS.md rule 3 proposal route — WS-02 should fold the matrix row below into
the q8 table.

AGENTS.md rule 2 requires that a new tool record what the existing Epic equivalent does
and why it is insufficient. Epic **does** have an equivalent here. This is the argument
that the new tool is a composite rather than a duplicate.

## The Epic equivalent

`editor_toolset.toolsets.blueprint.BlueprintTools` ships three node-discovery tools
`[VERIFIED: docs/audit/raw/schemas/editor_toolset.toolsets.blueprint.BlueprintTools.json]`:

| Epic tool | Inputs | Returns |
|---|---|---|
| `find_node_categories` | `graph`, `category_filter`, `context_pins` | matching palette categories |
| `find_node_types` | `graph`, `type_id_filter`, `context_pins` | matching node `type_id`s |
| `get_node_type_pins` | `graph`, **`type_id`** | `NodeInfo` with `input_pins` / `output_pins` for **one** node type |

They work. They are not being superseded as primitives — disposition is **preserve**.

## Why it is insufficient as the agent-facing surface

**1. Pin discovery is one call per node type.** `get_node_type_pins` takes a single
`type_id`. An agent that finds 25 candidate nodes and needs their pin names before it can
author a `submit_graph` payload spends `1 + 25 = 26` MCP round trips. `describe_node_catalog`
returns the same 25 entries *with* pins in **1**.

This is precisely the case `docs/WHY.md` is about: agent cost is superlinear in call
count and only linear in payload size. A 26-call discovery phase is the expensive shape;
one fat response is the cheap one. Rule 5's inspect → mutate → inspect test is failed by
the Epic sequence and passed by the composite.

**2. `graph` is a UObject `refPath`, not a stable asset path.** Every Epic node tool
takes a live `/Script/Engine.EdGraph` object reference. The agent must already hold a
graph handle, which it can only get from another call. `describe_node_catalog` takes
`target.asset_path` + `target.graph_id`, the same addressing `read_graph` and
`submit_graph` use — so the three tools compose without a handle-fetching step.
This limitation is already recorded for `BlueprintTools` in the q8 matrix
("UObject pin refs; no envelope").

**3. No ADR-0003 envelope.** Epic's tools return bare payloads: no `status`, no
`capability_notes`, no `validation.checks_performed`. In particular there is no way to
say "this node type exists but I could not build a template for it" — the new tool
reports that as `pins_resolved: false` per entry plus a `pins_unresolved` count, rather
than silently omitting pins (rule 6).

**4. Pin type fidelity.** The q8 audit already flags that Epic's `PinInfo.type_id` is a
*display string*, not an `FEdGraphPinType`
`[VERIFIED: docs/audit/epic-toolsets.md:150]`. The catalog returns the structured
components — `category`, `sub_category`, `sub_category_object`, `container_type`,
`is_reference` — which is what `graph.schema.json` edges actually need.

## Why it reads `FBlueprintActionDatabase` directly instead of composing Epic's tools

The q8 disposition says WS-06 should "compose over `BlueprintTools` rather than rebuild
primitive node tools". Composing here would mean issuing Epic's 26-call sequence
*internally*, which fixes the agent's cost but not the editor's.

`FBlueprintActionDatabase` is the source Epic's own tools and the editor palette read
from, so going direct cannot drift from what the editor will actually place, and it is a
single in-process sweep. Rule 5 explicitly permits internal primitives — the constraint
is on the agent-facing surface, and that surface is one call.

No primitive node/pin tool is added or re-exposed. `create_node`, `connect_pins` and the
rest stay Epic's.

## Verified API surface

| Claim | Tag |
|---|---|
| `FBlueprintActionDatabase::Get()` / `GetAllActions()` returning `FActionRegistry` | `[VERIFIED: Editor/BlueprintGraph/Public/BlueprintActionDatabase.h:66,94]` |
| `UBlueprintNodeSpawner::NodeClass`, `DefaultMenuSignature`, `PrimeDefaultUiSpec`, `GetTemplateNode` | `[VERIFIED: Editor/BlueprintGraph/Public/BlueprintNodeSpawner.h:149,152,178,235]` |
| `UEdGraphNode::Pins` | `[VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphNode.h:293]` |
| `UEdGraphPin::PinName/Direction/PinFriendlyName/PinType/DefaultValue` | `[VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphPin.h:306,312,373,383,386]` |
| `FEdGraphPinType::PinCategory/PinSubCategory/PinSubCategoryObject/ContainerType/bIsReference` | `[VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphPin.h:82,86,90,101,111]` |
| `EEdGraphPinDirection` values `EGPD_Input` / `EGPD_Output` | `[VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphNode.h:99-101]` |
| `EPinContainerType` values `None/Array/Set/Map` | `[VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphNode.h:123-129]` |

All engine reads are against `C:\Program Files\Epic Games\UE_5.8\Engine\Source`.

## Proposed matrix row for `docs/audit/epic-toolsets.md` q8

| Capability | Epic tool(s) | Altitude | Disposition | Superseded by | Limitation |
|---|---|---|---|---|---|
| Node type + pin discovery | `find_node_types`, `find_node_categories`, `get_node_type_pins` | composite (per-type) | **preserve** primitives; **supersede** agent surface | `blueprints.describe_node_catalog` | pins are 1 call per type; `graph` is a UObject refPath; `type_id` pin types are display strings; no envelope |

## Limitations of the new tool (stated, not omitted)

- **Not context-pin filtered.** Epic's `context_pins` narrows results to nodes
  compatible with a pin you intend to wire to. The catalog has no equivalent yet; an
  agent wiring into a specific pin still has to filter client-side. Worth adding.
- **`total_scanned` counts spawners, not distinct node types.** One node class can back
  many spawners (every `K2Node_CallFunction` target is its own spawner), so the number is
  a cost signal, not a type count.
- **Template instantiation is capped by `max_results`.** Entries beyond the cap are
  counted in `total_matched` but not returned; `truncated: true` says so. This is
  deliberate — priming every spawner in the database is thousands of node allocations.
- **`pins_resolved: false` is possible.** Some spawners decline to produce a template for
  a given graph. Reported per entry rather than guessed.
- **Not yet run in a live editor.** The automation tests in
  `Private/Tests/UeremcpBlueprintNodeCatalogTests.cpp` are written but unexecuted; see the
  handoff note in the branch summary.
