# Capability Matrix — Epic Engine Toolsets (UE 5.8)

- **Owner:** WS-02
- **Status:** **source_complete** — tool names and dispositions verified from source; **runtime enumeration not done** (see checklist below)
- **Brief:** [RB-02](../research/RB-02-epic-toolset-inventory.md)
- **Last verified:** 2026-07-29

## Enumeration method

| Layer | Result | Tag |
|---|---|---|
| Runtime `list_toolsets` / `describe_toolset` | **Failed** — editor MCP on `127.0.0.1:8000` not reachable; proxy on `:8001` returns transport WinError 10061 | [VERIFIED-RUNTIME: `docs/audit/raw/runtime-negative-findings.json`] |
| Static source scan | **875 AICallable tools** across 25 toolset plugins (599 Python `@tool_call`, 274 C++ `UFUNCTION(meta=(AICallable))`) | [VERIFIED: `docs/audit/raw/source-scan-summary.json`] |
| Per-plugin dumps | `docs/audit/raw/plugins/<PluginName>.json` | [VERIFIED: generated from scan 2026-07-29] |

> **Do not cite JSON Schema shapes from this file** — runtime `describe_toolset` dumps were not obtained. Tool *names* and class structure are verified from source; parameter schemas need a live editor pass.

---

## Load state — RE project (RB-02 q1)

`RE.uproject` enables `AllToolsets` plus several toolsets individually
`[VERIFIED: RE.uproject plugin list]`. `AllToolsets` is an aggregator that enables
21 domain toolsets including `NiagaraToolsets`, `GASToolsets`, `GameplayTagsToolset`,
`PCGToolset`, `SemanticSearchToolset`, and others **not named directly** in the uproject
`[VERIFIED: AllToolsets/AllToolsets.uplugin]`.

| Source | Toolsets expected loaded when editor runs |
|---|---|
| Via `AllToolsets` (21) | AIModule, AnimationAssistant, AutomationTest, ConfigSettings, Conversation, DataRegistry, DataflowAgent, Editor, GameFeatures, GameplayTags, GAS, MCPClient, Niagara, PCG, Physics, Plugin, SemanticSearch, SlateInspector, StateTree, UMG, WorldConditions |
| Enabled outside `AllToolsets` in RE.uproject | ChaosClothAsset, LiveCoding, MetaHumanGenerator, SequencerAnimMixer, HairModeling (plugin path not in standard Toolsets tree on this install), plus redundant re-listing of several AllToolsets members |
| Aggregator only | `AllToolsets` — no tools |
| No AICallable tools found | `MCPClientToolset` — editor MCP *client* subsystem/settings only `[VERIFIED: MCPClientToolset headers — no AICallable UFUNCTION]` |
| ToolsetRegistry reference | `UAgentSkillToolset` — ListSkills, GetSkills, CreateSkill, UpdateSkill `[VERIFIED: AgentSkill.h:69-103]` |

**Runtime confirmation pending:** actual registry contents when the editor is up.

---

## Priority answers (WS-05 / WS-06)

### q7 — `ProgrammaticToolset.execute_tool_script`

**Exists.** Registered on `ProgrammaticToolset` inside EditorToolset.

| Field | Value | Tag |
|---|---|---|
| Tool | `execute_tool_script` | [VERIFIED: programmatic.py:906] |
| Companion | `get_execution_environment` — must be called once before first script use | [VERIFIED: programmatic.py:887] |
| Input | `script: str` — Python defining `run()` → `dict[str, Any]` | [VERIFIED: programmatic.py:924-926] |
| Return | `ToolCallAsyncResultString` (async) — JSON string of `run()` result | [VERIFIED: programmatic.py:906,929] |
| In-script API | `execute_tool(full_dotted_name, json_input_str)` — batches registered tools | [VERIFIED: programmatic.py:295-320,473-512] |
| Safety | Sandboxed imports; read-only `open`; editor transaction with undo on failure | [VERIFIED: programmatic.py:352-385,764-877] |

Full dump: `docs/audit/raw/q7-programmatic-execute-tool-script.json`.

**Disposition:** `preserve` — WS-05 should treat this as the engine batching primitive to compose under `execute_plan`, not reinvent.

### q8 — Blueprint graph / node tools

**No `BlueprintNodeTools` class.** All graph authoring is on `BlueprintTools` (52 tools).

| Capability | Epic tool(s) | Altitude | Tag |
|---|---|---|---|
| Create BP asset | `create` | primitive | [VERIFIED: blueprint.py:177] |
| Node create/delete/wire | `create_node`, `delete_node`, `connect_pins`, `break_pins`, pin value ops | primitive | [VERIFIED: blueprint.py tool list] |
| Graph DSL round-trip | `read_graph_dsl`, `write_graph_dsl`, `get_graph_dsl_docs` | composite | [VERIFIED: blueprint.py:1440-1502] |
| Compile | `compile_blueprint` | primitive | [VERIFIED: blueprint.py:199] |
| Inspection | `find_nodes`, `get_node_infos`, `get_connected_subgraph`, `find_node_types` | composite | [VERIFIED: blueprint.py tool list] |

**Ceiling:** Epic already authors and reads full graphs (DSL + primitives) and compiles. It does **not** provide ADR-0003 envelopes, stable graph IDs/revisions/content hashes, or verified status taxonomy — that remains UEREMCP scope.

Full dump: `docs/audit/raw/q8-blueprint-graph-tools.json`.

**Disposition:** `preserve` primitives + DSL tools; `supersede` only the *agent-facing* surface with envelope graph submit/read/validate (WS-06 POC A).

---

## Toolset summary matrix

One row per **toolset class** (875 tools total). Full tool name lists: `docs/audit/raw/plugins/`.

| Plugin | Toolset class(es) | Tools | Altitude mix | Disposition | Superseded by (UEREMCP) | Limitations | Tag |
|---|---|---|---|---|---|---|---|
| EditorToolset | `BlueprintTools` | 52 | primitive + DSL composite | preserve / supersede surface | `blueprints.submit_graph`, `blueprints.read_graph` (planned) | UObject pin refs; no envelope; DSL not JSON graph schema | [VERIFIED: source scan] |
| EditorToolset | `ProgrammaticToolset` | 2 | composite (batch) | preserve | compose in `execute_plan` | Async only; script sandbox; no arbitrary imports | [VERIFIED: programmatic.py] |
| EditorToolset | `MaterialTools` | 22 | primitive | internalise | compose in `create_vfx_material`, `retrieve/replace_material_graph` (WS-08) | Per-expression ops; UObject refs; no ADR-0004 graph JSON | [VERIFIED: material.py, source scan] |
| EditorToolset | `MaterialInstanceTools` | 13 | primitive | preserve / improve via envelope | `create_vfx_material` batches MI params (WS-08) | Scalar/vector/texture/switch overrides; static switch recompile cost | [VERIFIED: material_instance.py, source scan] |
| EditorToolset | `ActorTools`, `SceneTools`, `AssetTools`, `ObjectTools`, … | 174 | primitive–composite | internalise | goal-level domain tools | Many round-trips for workflows | [VERIFIED: source scan] |
| EditorToolset | `UEditorAppToolset`, `ULogsToolset` (C++) | 25 | primitive | preserve | — | Editor/PIE/log plumbing | [VERIFIED: EditorAppToolset.h, LogsToolset.h] |
| NiagaraToolsets | C++ Niagara BP API | 56 | primitive–composite | preserve / internalise | `niagara.submit_graph` (planned) | Stack/module topology ops; batched via execute_tool_script in REAgentTools | [VERIFIED: source scan] |
| GASToolsets | C++ GAS inspection | 14 | composite | preserve | `gameplay.*` (planned) | Cue/tag inspection; not ability graph authoring | [VERIFIED: source scan] |
| GameplayTagsToolset | C++ tag CRUD | 6 | primitive | preserve | — | Tag dictionary ops only | [VERIFIED: source scan] |
| PCGToolset | C++ graph + instance | 31 | primitive–composite | preserve | defer | PCG graph authoring exists | [VERIFIED: source scan] |
| AnimationAssistantToolset | ControlRig, Sequencer*, etc. | 300+ | primitive–composite | preserve / defer | WS-10 | Broad sequencer/control-rig surface | [VERIFIED: source scan] |
| UMGToolSet | C++ widget BP | 23 | primitive–composite | defer | — | Widget tree authoring | [VERIFIED: source scan] |
| StateTreeToolset | `StateTreeTools` | 9 | composite | defer | — | Read-heavy state inspection | [VERIFIED: source scan] |
| SemanticSearchToolset | C++ | 2 | composite | preserve | template search (WS-15) | `Search`, `FindSimilar` only | [VERIFIED: source scan] |
| AutomationTestToolset | C++ | varies | composite | preserve | WS-11 harness | Potential test driver | [VERIFIED: source scan] |
| SlateInspectorToolset | C++ UI automation | 14 | primitive | preserve | reliability (RB-13) | Dialog/form driving | [VERIFIED: source scan] |
| DataflowAgent | C++ graph | 22 | primitive–composite | defer | — | Dataflow graph ops | [VERIFIED: source scan] |
| PhysicsToolsets | C++ | 17 | primitive | defer | — | Physics asset bodies/constraints | [VERIFIED: source scan] |
| PluginToolset | C++ | 17 | composite | defer | — | Plugin descriptor management | [VERIFIED: source scan] |
| MCPClientToolset | — | 0 | — | preserve (infra) | — | No agent tools; MCP client config | [VERIFIED: header scan] |
| AllToolsets | — | 0 | — | preserve (infra) | — | Dependency aggregator | [VERIFIED: AllToolsets.uplugin] |
| ToolsetRegistry | `UAgentSkillToolset` | 4 | goal (skills) | preserve | WS-15 templates | Skill assets, not domain graphs | [VERIFIED: AgentSkill.h] |
| Remaining plugins | see raw/ | see raw | — | defer | — | ChaosCloth, Config, Conversation, DataRegistry, GameFeatures, LiveCoding, MetaHuman, SequencerAnimMixer, WorldConditions, AIModule | [VERIFIED: source scan] |

---

## Materials — WS-08 disposition

Accepted from `docs/proposals/ws-08-epic-material-audit.md` (WS-01, 2026-07-29).
Tool names cross-checked against `docs/audit/raw/plugins/EditorToolset.json`
`[VERIFIED: material.py, material_instance.py, source scan 2026-07-29]`. No runtime
schemas — dispositions are source + accepted architecture only.

### Epic `MaterialTools` — internalise (hide from agent via `SetNameFilters`)

| Tool | Purpose | Disposition | Superseded by | Tag |
|---|---|---|---|---|
| `create_material` | Empty material asset | internalise | `create_vfx_material` | [VERIFIED: material.py] |
| `create_function` | Empty material function | internalise | graph replace path | [VERIFIED: material.py] |
| `create_parameter_collection` | MPC asset | internalise | rare direct use | [VERIFIED: material.py] |
| `list_expression_classes` | Discover expression types | internalise | semantic tool class pick | [VERIFIED: material.py] |
| `add_expression` | Add graph node | internalise | `replace_material_graph` | [VERIFIED: material.py] |
| `delete_expression` | Remove graph node | internalise | same | [VERIFIED: material.py] |
| `get_expressions` | List nodes | internalise / graph-read adapter | `retrieve_material_graph` | [VERIFIED: material.py] |
| `layout_expressions` | Auto-layout | internalise (optional) | — | [VERIFIED: material.py] |
| `list_parameter_groups` | Parameter UI groups | internalise / graph read | `retrieve_material_graph` | [VERIFIED: material.py] |
| `rename_parameter_group` | Group rename | internalise | — | [VERIFIED: material.py] |
| `delete_parameter_group` | Ungroup parameters | internalise | — | [VERIFIED: material.py] |
| `get_expression_input_names` | Pin discovery | internalise / graph read | `retrieve_material_graph` | [VERIFIED: material.py] |
| `get_expression_output_names` | Pin discovery | internalise / graph read | same | [VERIFIED: material.py] |
| `connect_expressions` | Wire nodes | internalise | `replace_material_graph` | [VERIFIED: material.py] |
| `disconnect_expressions` | Unwire pin | internalise | same | [VERIFIED: material.py] |
| `get_expression_inputs` | Read wiring | internalise / graph read | `retrieve_material_graph` | [VERIFIED: material.py] |
| `get_property_input` | Read output property source | internalise / graph read | same | [VERIFIED: material.py] |
| `connect_to_output` | Wire to MP_* | internalise | `replace_material_graph` | [VERIFIED: material.py] |
| `disconnect_from_output` | Unwire MP_* | internalise | same | [VERIFIED: material.py] |
| `delete_unused_expressions` | Cleanup | internalise | — | [VERIFIED: material.py] |
| `recompile` | Shader compile + errors | internalise → validation layer | WS-11 compile gate | [VERIFIED: material.py] |
| `get_referencing_materials` | Function referencers | internalise | diagnostics | [VERIFIED: material.py] |

### Epic `MaterialInstanceTools` — preserve / improve via envelope

| Tool | Purpose | Disposition | Notes | Tag |
|---|---|---|---|---|
| `create` | Create MIC | improve | envelope + idempotency (ADR-0003/0006) | [VERIFIED: material_instance.py] |
| `list_parameters` | Parameter manifest | improve | return in semantic response | [VERIFIED: material_instance.py] |
| `get_scalar_parameter` / `set_scalar_parameter` | Scalar MI override | improve | batch in `create_vfx_material` | [VERIFIED: material_instance.py] |
| `get_vector_parameter` / `set_vector_parameter` | Vector MI override | improve | batch in `create_vfx_material` | [VERIFIED: material_instance.py] |
| `get_texture_parameter` / `set_texture_parameter` | Texture MI override | improve | batch in `create_vfx_material` | [VERIFIED: material_instance.py] |
| `get_static_switch_parameter` / `set_static_switch_parameter` | Static switch | improve | warn on recompile cost | [VERIFIED: material_instance.py] |
| `set_parent` | Reparent MI | internalise | — | [VERIFIED: material_instance.py] |
| `clear_parameters` | Reset overrides | internalise | dry_run default (ADR-0008 pattern) | [VERIFIED: material_instance.py] |
| `set_parameter_override` | Toggle override flag | internalise | — | [VERIFIED: material_instance.py] |

### UEREMCP material actions (not duplicates — real gaps)

| Planned action | Why not duplicate Epic/RE | Owner |
|---|---|---|
| `create_vfx_material` | One semantic op; batches MaterialTools + MI + validation | WS-08 |
| `retrieve_material_graph` | ADR-0004 JSON; Epic returns UObject refs | WS-08 |
| `replace_material_graph` | ADR-0004 round-trip | WS-08 |
| `create_procedural_texture` | No Epic equivalent | WS-08 |
| `instantiate_element_material` | Element template + parameter model (ADR-0008) | WS-08 / WS-15 |

Coordinate elemental Niagara+material templates with WS-07 and WS-15 per accepted proposal.

---

## Do-not-rebuild list

Tools already at composite/goal altitude or that would duplicate working Epic surface:

1. **`ProgrammaticToolset.execute_tool_script`** — engine batching primitive with transaction safety `[VERIFIED: programmatic.py]`
2. **`BlueprintTools.read_graph_dsl` / `write_graph_dsl`** — graph logic round-trip `[VERIFIED: blueprint.py:1454-1502]`
3. **`BlueprintTools` node/pin primitives** — create/connect/compile pipeline `[VERIFIED: source scan — 52 tools]`
4. **`NiagaraToolsets.*`** — system/emitter/module/renderer stack authoring (56 tools) `[VERIFIED: source scan]`
5. **`MaterialTools.*`** — master material expression graph (22 tools); **internalise**, do not expose `[VERIFIED: material.py, WS-08 proposal]`
6. **`MaterialInstanceTools.*`** — MI parameter CRUD (13 tools); improve via envelope, do not duplicate per-param agent tools `[VERIFIED: material_instance.py, WS-08 proposal]`
7. **`SemanticSearchToolset.Search` / `FindSimilar`** — project semantic asset search `[VERIFIED: source scan]`
8. **`ULogsToolset.GetLogEntries`** and editor/PIE helpers on `UEditorAppToolset` `[VERIFIED: LogsToolset.h, EditorAppToolset.h]`
9. **`SlateInspectorToolset`** — UI dialog automation `[VERIFIED: source scan]`
10. **`UAgentSkillToolset`** — skill template CRUD `[VERIFIED: AgentSkill.h]`
11. **REAgentTools `execute_editor_batch`** — prior-art batch with `$ref` chaining; audit disposition in RB-15 / `reagenttools.md` (not Epic, but do not rebuild batch grammar without reading it)

---

## Real gaps (no Epic equivalent)

Capabilities where Epic + REAgentTools still leave holes — justified new UEREMCP work:

| Gap | Why Epic is insufficient | Owner |
|---|---|---|
| Envelope-shaped request/response (ADR-0003) | Epic tools are per-call JSON schemas, not versioned envelopes | WS-05 |
| Complete graph JSON with stable IDs, revision, content_hash (ADR-0004/0006) | Blueprint DSL uses text + UObject refs; Niagara/PCG use tool-specific topology, not unified graph.schema | WS-06, WS-07 |
| Verified status taxonomy | Epic returns tool errors/values; no `created_and_validated` pipeline | WS-11 |
| Idempotent goal-level domain actions | Stable paths + expected_revision not on Epic surface | WS-05, domain WS |
| Multi-asset rollback semantics | `execute_tool_script` undoes single transactional script, not FileSandbox batch | WS-11, WS-12 |
| GAS **ability graph** authoring | GASToolsets inspect cues/tags/effects; no ability graph construction | WS-09 |
| Semantic VFX **material templates** | MaterialTools are per-expression; REAgentTools MI-only — need `create_vfx_material`, graph JSON, procedural texture, element instantiation | WS-08 |
| Project-specific RE workflows | dress/character/lighting REAgentTools toolsets | defer / project layer |

---

## Coverage assertion

Planned UEREMCP surface must **cover every `supersede` row** above (envelope graph ops for Blueprint/Niagara/Materials) while **preserving** Epic primitives internally via `SetNameFilters` (ADR-0002). Until POC A–C land, coverage is **claimed but not built**.

---

## REAgentTools cross-check (RB-15 pointers)

| REAgentTools claim | Epic audit result |
|---|---|
| `ProgrammaticToolset.execute_tool_script` for Niagara batching | **Confirmed** in source `[VERIFIED: programmatic.py]` — runtime not exercised |
| `BlueprintTools` + `BlueprintNodeTools` | **Partially wrong** — only `BlueprintTools` exists `[VERIFIED: __init__.py]` |
| `NiagaraToolsets.*` available | **Expected** when `AllToolsets` loads `[VERIFIED: AllToolsets.uplugin]` — runtime not confirmed |
| `MaterialTools` for master materials | **Confirmed** 22 tools `[VERIFIED: source scan]` |

---

## Runtime verification checklist

This file is **`source_complete`, not audit-complete.** The following require a live RE
editor with ModelContextProtocol on `127.0.0.1:8000/mcp` before the Epic audit can be
marked runtime-verified:

- [ ] `list_toolsets` — dump to `docs/audit/raw/runtime-list_toolsets.json`
- [ ] `describe_toolset` for each loaded class — schemas to `docs/audit/raw/schemas/`
- [ ] Confirm load set matches `AllToolsets` inference (Niagara, GAS, GameplayTags, etc.)
- [ ] Verify dotted toolset names match source scan (Python module paths vs MCP names)
- [ ] Classify async tools (confirm `execute_tool_script` async; sample others)
- [ ] Sample result payload sizes for composite tools (token budget calibration)
- [ ] Negative findings log updated or closed — `docs/audit/raw/runtime-negative-findings.json`

Until all checked: cite tool **names** from this file with `[VERIFIED: source scan]`; cite
**schemas and load state** only from runtime dumps once they exist.

---

## Open blockers

1. **Editor MCP offline** — re-run `list_toolsets` + `describe_toolset` for every loaded class; store verbatim schemas in `docs/audit/raw/schemas/`.
2. **Runtime load set** — confirm registry matches AllToolsets inference when RE editor runs.
3. **JSON Schema dumps** — q7/q8 input schemas are from Python annotations, not registry-generated schemas (RB-03 q6 coupling).

---

## Unverified claims — do not propagate

- Exact dotted toolset names at MCP discovery (e.g. `EditorToolset.AssetTools` vs `editor_toolset.toolsets.asset.AssetTools`) — needs runtime `list_toolsets`.
- Async vs sync classification per tool — `execute_tool_script` is async `[VERIFIED: return type]`; others need runtime or header read per tool.
- Result payload sizes — need live calls.
