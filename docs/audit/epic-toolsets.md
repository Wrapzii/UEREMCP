# Capability Matrix — Epic Engine Toolsets (UE 5.8)

- **Owner:** WS-02
- **Status:** **runtime_partial** — priority Epic + UEREMCP toolsets have `describe_toolset` JSON dumps; full 73-toolset matrix not schema-complete
- **Brief:** [RB-02](../research/RB-02-epic-toolset-inventory.md)
- **Last verified:** 2026-07-30 (runtime schemas for priority matrix)

## Enumeration method

| Layer | Result | Tag |
|---|---|---|
| Runtime `list_toolsets` / `describe_toolset` | **Partial** — full registry dump + 12 priority `describe_toolset` schemas on 2026-07-30; 61 toolsets still schema-only | [VERIFIED-RUNTIME: `docs/audit/raw/runtime/list_toolsets.json`, `docs/audit/raw/schemas/`, `capture-metadata.json` 2026-07-30] |
| Static source scan | **875 AICallable tools** across 25 toolset plugins (599 Python `@tool_call`, 274 C++ `UFUNCTION(meta=(AICallable))`) | [VERIFIED: `docs/audit/raw/source-scan-summary.json`] |
| Per-plugin dumps | `docs/audit/raw/plugins/<PluginName>.json` | [VERIFIED: generated from scan 2026-07-29] |

> **Cite JSON Schema shapes** from `docs/audit/raw/schemas/<toolset>.json` for the 12
> priority toolsets listed in `docs/audit/raw/runtime/capture-metadata.json`
> `[VERIFIED-RUNTIME: describe_toolset 2026-07-30]`. Other toolsets: tool *names* from
> source scan; schemas still need runtime dumps.

**Runtime MCP naming (verified):** Python EditorToolset classes register as
`editor_toolset.toolsets.<module>.<Class>` (e.g.
`editor_toolset.toolsets.blueprint.BlueprintTools`), not `EditorToolset.BlueprintTools`
`[VERIFIED-RUNTIME: list_toolsets 2026-07-30]`. C++ plugin toolsets use
`PluginName.ClassName` (e.g. `GASToolsets.GameplayCueToolset`).

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

**Runtime confirmation (2026-07-30):** `list_toolsets` returns **73** registered toolsets on
live RE — Epic AllToolsets members, C++ plugins, Python `editor_toolset.toolsets.*`, 15
REAgentTools workflow toolsets, `UeremcpCore.UeremcpReferenceToolset`, and extras (MetaHuman,
SequencerAnimMixer, MVVM, Conversation, AIModule BT) `[VERIFIED-RUNTIME: list_toolsets.json]`.
Priority `describe_toolset` schemas captured for Blueprint, Niagara System/Assets, GAS,
GameplayTags, Materials, ControlRig sample, Programmatic, and UEREMCP reference
`[VERIFIED-RUNTIME: docs/audit/raw/schemas/ 2026-07-30]`.

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
| EditorToolset | `BlueprintTools` | 52 | primitive + DSL composite | preserve / supersede surface | `blueprints.submit_graph`, `blueprints.read_graph` (planned) | UObject pin refs; no envelope; DSL not JSON graph schema | [VERIFIED: source scan; WS-06 proposal] — detail: Blueprint WS-06 disposition |
| EditorToolset | `ProgrammaticToolset` | 2 | composite (batch) | preserve | compose in `execute_plan` | Async only; script sandbox; no arbitrary imports | [VERIFIED: programmatic.py] |
| EditorToolset | `MaterialTools` | 22 | primitive | internalise | compose in `create_vfx_material`, `retrieve/replace_material_graph` (WS-08) | Per-expression ops; UObject refs; no ADR-0004 graph JSON | [VERIFIED: material.py, source scan] |
| EditorToolset | `MaterialInstanceTools` | 13 | primitive | preserve / improve via envelope | `create_vfx_material` batches MI params (WS-08) | Scalar/vector/texture/switch overrides; static switch recompile cost | [VERIFIED: material_instance.py, source scan] |
| EditorToolset | `ActorTools`, `SceneTools`, `AssetTools`, `ObjectTools`, … | 174 | primitive–composite | internalise | goal-level domain tools | Many round-trips for workflows | [VERIFIED: source scan] |
| EditorToolset | `UEditorAppToolset`, `ULogsToolset` (C++) | 25 | primitive | preserve | — | Editor/PIE/log plumbing | [VERIFIED: EditorAppToolset.h, LogsToolset.h] |
| NiagaraToolsets | C++ Niagara BP API | 56 | primitive–composite | preserve / internalise | `niagara.submit_graph` (planned) | Stack/module topology ops; batched via execute_tool_script in REAgentTools | [VERIFIED: source scan] |
| GASToolsets | `GameplayCueToolset`, `AttributeSetToolset`, `AbilitySystemInspectorToolset` | 14 | primitive–composite | preserve / improve (cue create) | WS-09 `create_spell`, `upsert_ability_row` | No `UGameplayAbility` / `UGameplayEffect` authoring; RE has no ASC on characters | [VERIFIED: source scan; WS-09 proposal] |
| GameplayTagsToolset | C++ tag CRUD | 6 | primitive | preserve with policy | — | INI mutation; concurrency hazard on rename | [VERIFIED: source scan; GameplayTagsToolset.cpp:93-147] |
| EditorToolset | `DataTableTools` | 7+ | mid | preserve | WS-09 `upsert_ability_row` (idempotency wrapper) | Whole-table JSON rewrite on `set_rows`; no revision API | [VERIFIED: data_table.py:59-241] |
| PCGToolset | C++ graph + instance | 31 | primitive–composite | preserve | defer | PCG graph authoring exists | [VERIFIED: source scan] |
| AnimationAssistantToolset | `ControlRigTools` (44), Sequencer* (276) | 320 | primitive–composite | preserve / compose | `animation.*` / `control_rig.*` (Wave 3) | RigVM primitives; no AnimBP/montage/notify goal ops | [VERIFIED: controlrig.py + audit JSON; WS-10 proposal] |
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

## Blueprint — WS-06 disposition

Accepted from `docs/proposals/ws-06-audit-blueprint-rows.md` (WS-06 → WS-02, 2026-07-29).
Evidence: [RB-05](../research/RB-05-blueprint-graph-ceiling.md); runtime probes noted in RB-05.
Cross-checked against `docs/audit/raw/plugins/EditorToolset.json` and q8 dump
`[VERIFIED: blueprint.py, helpers.py, source scan 2026-07-29]`. Per-tool MCP schemas
still need runtime `describe_toolset` — dispositions are source + RB-05 only.

**Correction (propagate):** REAgentTools and older docs sometimes name `BlueprintNodeTools`.
That class **does not exist** in UE 5.8 EditorToolset — only `BlueprintTools`
`[VERIFIED: docs/audit/raw/q8-blueprint-graph-tools.json; EditorToolset __init__.py]`.

**WS-06 commitment:** WS-06 will not reimplement create/connect/compile pin primitives.
New work is envelope mapping, hashing, diagnostics, and verification only.

### Epic `BlueprintTools` (52 tools)

| Toolset / tools | Altitude | Disposition | Superseded by (UEREMCP) | Limitations | Tag |
|---|---|---|---|---|---|
| `BlueprintTools` (52) | primitive + DSL composite | **preserve** primitives; **supersede** agent surface | `blueprints.read_graph`, `blueprints.submit_graph` | UObject pin refs; DSL ≠ `graph.schema.json`; no envelope/hash/revision | `[VERIFIED: blueprint.py]` + RB-05 |
| `read_graph_dsl` / `write_graph_dsl` | composite | **preserve** as *internal* backend | composed under submit/read | MultiGate decompile fail; bind elision; no semantic_id | `[VERIFIED: blueprint.py:1454-1502]` `[VERIFIED-RUNTIME: RB-05]` |
| `get_node_infos` / `find_nodes` / `get_connected_subgraph` | composite inspect | **preserve** / internalise | feed structured read | `PinInfo.type_id` is display string, not `FEdGraphPinType` | `[VERIFIED: blueprint.py:609-748]` |
| `compile_blueprint` | primitive | **preserve** | validation pipeline | Sync; errors via `ErrorMsg` / `list_nodes_with_errors` | `[VERIFIED: helpers.py:30-44]` |
| `ProgrammaticToolset.execute_tool_script` | batch | **preserve** | compose under `execute_plan` | Required for multi-tool Blueprint jobs without MCP thrash | WS-02 q7; `[VERIFIED: programmatic.py:906]` |

### UEREMCP blueprint actions (not duplicates — envelope layer)

| Planned action | Why not duplicate Epic/RE | Owner |
|---|---|---|
| `blueprints.read_graph` | ADR-0004 JSON + stable IDs/revision/hash; Epic DSL is text + UObject refs | WS-06 |
| `blueprints.submit_graph` | Same + verified status taxonomy after compile/save | WS-06 |

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

## Gameplay / GAS — WS-09 disposition

Accepted from `docs/proposals/ws-09-audit-gas-toolsets.md` (WS-09, 2026-07-29). Tool names
cross-checked against `docs/audit/raw/plugins/GASToolsets.json`, `GameplayTagsToolset.json`,
and `EditorToolset.json` `[VERIFIED: headers + source scan 2026-07-29]`. Brief:
[RB-12](../research/RB-12-gas-and-gameplay.md). Per-tool schemas still need runtime
`describe_toolset` — dispositions are source + accepted architecture only.

### `GASToolsets.GameplayCueToolset`

| Tool(s) | Purpose | Altitude | Disposition | Superseded by / notes | Tag |
|---|---|---|---|---|---|
| `ListCues`, `GetCueInfo`, `FindCueNotifyAssets`, `FindCueTagsWithoutNotifies` | Inspect cue tags and notify assets | primitive | **preserve** | — | [VERIFIED: GameplayCueToolset.h:67-145] |
| `ExecuteCueOnSelectedActor` | Non-replicated cue preview on selection | primitive | **preserve** | Needs selection; not net validation | [VERIFIED: GameplayCueToolset.h:91-92] |
| `CreateCueNotifyAsset` | Create empty Static/Actor notify BP; set tag on CDO | primitive | **improve** (compose into goal-level with VFX) | No Niagara/audio bind; tag must pre-exist | WS-09 `create_spell` presentation + WS-07 | [VERIFIED: GameplayCueToolset.cpp:218-286] |
| `AddCueTag`, `RemoveCueTag` | Mutate `GameplayCue.*` tags in INI | primitive | **preserve** with policy | Concurrent-agent INI race; destructive | ADR-0006 tag policy | [VERIFIED: GameplayCueToolset.cpp:289-338] |

### `GASToolsets.AttributeSetToolset`

| Tool(s) | Purpose | Altitude | Disposition | Notes | Tag |
|---|---|---|---|---|---|
| `FindAttributeSetClasses`, `ListAttributes` | Discover AttributeSet types and fields | primitive | **preserve** | No create/edit; RE project has no GAS attribute sets in use | [VERIFIED: AttributeSetToolset.h:54-70] |

### `GASToolsets.AbilitySystemInspectorToolset`

| Tool(s) | Purpose | Altitude | Disposition | Notes | Tag |
|---|---|---|---|---|---|
| `GetAttributeValues`, `GetActiveEffects`, `GetGrantedAbilities`, `GetActiveTags` | Runtime ASC inspection | primitive | **preserve** | Requires ASC; RE magecraft characters have none today | [VERIFIED: AbilitySystemInspectorToolset.h:101-131] |

### `GameplayTagsToolset`

| Tool(s) | Purpose | Altitude | Disposition | Notes | Tag |
|---|---|---|---|---|---|
| `ListTags`, `GetTagInfo`, `FindReferencersByTag` | Tag discovery | primitive | **preserve** | — | [VERIFIED: GameplayTagsToolset.h:44-89] |
| `AddTag`, `RemoveTag`, `RenameTag` | INI tag mutation | primitive | **preserve** with policy | Concurrency hazard; rename rewrites referencers | ADR-0006 | [VERIFIED: GameplayTagsToolset.cpp:93-147] |

### Epic `DataTableTools` — substrate for RE abilities (not GAS)

| Tool(s) | Purpose | Altitude | Disposition | Notes | Tag |
|---|---|---|---|---|---|
| `create`, `add_rows`, `set_rows`, `get_rows`, `list_rows`, … | DataTable CRUD | mid | **preserve** | Whole-table JSON rewrite on `set_rows`; no revision API | WS-09 `upsert_ability_row` wraps with idempotency | [VERIFIED: data_table.py:59-241] |

### UEREMCP gameplay actions (not duplicates — real gaps)

| Planned action | Why not duplicate Epic/RE | Owner |
|---|---|---|
| `create_spell` | Batches DataTable row + VFX/cue presentation + validation; not a rename of GASToolsets | WS-09 |
| `upsert_ability_row` | Idempotent `FREAbilityDef` / `CastAbility` row semantics on top of `DataTableTools` | WS-09 |

Goal-level `create_spell` must wrap DataTable + VFX domains and validate against RE runtime —
Epic GAS toolsets do not understand `FREAbilityDef` or `CastAbility`.

---

## Animation and Control Rig — WS-10 disposition

Accepted from `docs/proposals/ws-10-animation-audit-rows.md` (WS-10 → WS-02, 2026-07-29).
Evidence: [RB-09](../research/RB-09-animation-controlrig.md); tool counts from AnimationAssistant audit JSON + source.

| Plugin / surface | Tools | Disposition | Superseded by (UEREMCP) | Limitations | Tag |
|---|---|---|---|---|---|
| AnimationAssistant `ControlRigTools` | 44 | **preserve / compose** | `animation.*` / `control_rig.*` goal ops (Wave 3) | RigVM primitives; not envelope | [VERIFIED: controlrig.py] + runtime create |
| AnimationAssistant Sequencer* | 276 | **preserve / internalise** | bake path only | No AnimBP/montage/notify | [VERIFIED: audit JSON 320 total] |
| EditorToolset `SkeletalMeshTools` sockets/bones | ~15 anim-relevant | **preserve** | `animation.ensure_socket`, skeleton inspect | — | [VERIFIED-RUNTIME] |
| `UAnimationBlueprintLibrary` | n/a (not a toolset) | **internalise** | montage/notify/marker ops | Editor module | [VERIFIED: AnimationBlueprintLibrary.h] |
| AnimBP state machine authoring | **none in Epic toolsets** | **gap** | read-only inspect until proven | Schema spawners only | negative finding |

### UEREMCP animation gaps (confirmed)

| Gap | Why Epic is insufficient | Owner |
|---|---|---|
| Goal-level montage + **real** AnimNotify authoring | `UAnimationBlueprintLibrary` exists; no Epic toolset exposes notify authoring | WS-10 |
| AnimBP / state-machine structured inspect as ADR-0004 JSON | `list_graphs` works; DSL empty for state machines | WS-10 |
| AnimBP state-machine **authoring** | Documented non-goal for Phase 4 | — |

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
11. **`GASToolsets.*` cue/tag/ASC inspection** — 14 C++ tools; improve only via goal-level compose (WS-09) `[VERIFIED: GASToolsets.json, WS-09 proposal]`
12. **`GameplayTagsToolset.*`** — tag dictionary CRUD with INI policy `[VERIFIED: GameplayTagsToolset, WS-09 proposal]`
13. **`DataTableTools.*`** — table CRUD substrate for ability rows; wrap, do not reimplement `[VERIFIED: data_table.py:59-241]`
14. **Entire AnimationAssistant ~320-tool surface** — compose via goal ops; do not re-expose RigVM/Sequencer primitives `[VERIFIED: WS-10 proposal]`
15. **`SkeletalMeshTools` socket/bone primitives** — preserve; wrap in `animation.ensure_socket` `[VERIFIED: WS-10 proposal]`
16. **Sequencer Control Rig keying** — compose via REAnimWorkflow bake pattern, do not rebuild `[VERIFIED: WS-10 proposal]`
17. **REAgentTools `execute_editor_batch`** — prior-art batch with `$ref` chaining; audit disposition in RB-15 / `reagenttools.md` (not Epic, but do not rebuild batch grammar without reading it)

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
| GAS **ability graph** authoring | GASToolsets inspect cues/tags/effects; no `UGameplayAbility` / `UGameplayEffect` creation | WS-09 |
| RE `FREAbilityDef` / `CastAbility` row authoring | No Epic tool understands RE ability table schema or cast pipeline | WS-09 |
| Goal-level spell creation | Not a rename of GASToolsets — must compose DataTable + VFX + validation | WS-09 |
| Semantic VFX **material templates** | MaterialTools are per-expression; REAgentTools MI-only — need `create_vfx_material`, graph JSON, procedural texture, element instantiation | WS-08 |
| Goal-level montage + **real** AnimNotify authoring | Library API exists; no Epic toolset; REAnim notify plan is metadata-only | WS-10 |
| AnimBP state-machine structured inspect (ADR-0004 JSON) | Epic `list_graphs` only; DSL empty for state machines | WS-10 |
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

Epic audit is **`runtime_partial`** for R-06 (priority matrix schema-complete; full matrix open).

- [x] `list_toolsets` — dump to `docs/audit/raw/runtime/list_toolsets.json` (73 toolsets, 2026-07-30)
- [x] `describe_toolset` for priority disposition toolsets — `docs/audit/raw/schemas/` (12 files)
- [x] Confirm `GASToolsets` + `GameplayTagsToolset` in `list_toolsets` (2026-07-30)
- [x] Confirm `UeremcpCore.UeremcpReferenceToolset` present with Ping/Echo schemas
- [ ] Confirm full load set matches `AllToolsets` inference — **partial** (73 enumerated; HairModeling not seen)
- [x] Verify dotted toolset names: Python `editor_toolset.toolsets.*` vs C++ `Plugin.Class` (documented above)
- [ ] Classify async tools per tool — **partial** (`execute_tool_script` string return only)
- [ ] Sample result payload sizes for composite tools
- [x] Negative findings log updated — `docs/audit/raw/runtime-negative-findings.json` (success entry 2026-07-30)

Until all checked: cite **schemas** only for toolsets in `capture-metadata.json`;
other toolsets remain `[VERIFIED: source scan]` for names only.

---

## Open blockers

1. **Remaining schema dumps** — `describe_toolset` for ~61 non-priority toolsets (PCG, UMG, Sequencer*, REAgentTools 15, etc.).
2. **Payload / async calibration** — live read-only calls for token budget and async classification.
3. **Blueprint tool count** — runtime 53 vs source scan 52; reconcile on next source scan.

---

## Unverified claims — do not propagate

- Exact dotted toolset names at MCP discovery — **resolved for EditorToolset Python classes**
  (`editor_toolset.toolsets.*`) and C++ plugins (`GASToolsets.*`, etc.)
  `[VERIFIED-RUNTIME: list_toolsets 2026-07-30]`.
- Async vs sync classification per tool — `execute_tool_script` is async `[VERIFIED: return type]`; others need runtime or header read per tool.
- Result payload sizes — need live calls.
