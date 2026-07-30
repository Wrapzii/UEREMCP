# Capability Catalog

**Owner:** WS-01. Workstreams register actions via `docs/proposals/`.

The registry of every agent-facing `action`. An action that is not listed here does not
exist as far as agents are concerned.

## How to register an action

1. Define its `specification` schema under `schemas/domains/<your-domain>/`.
2. Write a proposal to WS-01 with the row to add below.
3. Ship it with tests and an audit entry showing no Epic/REAgentTools duplication.

## Status values

| Status | Meaning |
|---|---|
| `planned` | Named in the design; not implemented |
| `research` | Feasibility unresolved; see the linked brief |
| `partial` | Works with documented limitations — `capability_notes` required on every response |
| `available` | Implemented, tested, verified |

## Discovery

Agents should not need this file at runtime. Epic's server already provides tool search
— `list_toolsets`, `describe_toolset`, `call_tool`, on by default
`[VERIFIED: ModelContextProtocolSettings.h]`. Our discovery actions layer semantic
grouping on top of it rather than replacing it (ADR-0002).

| Action | Domain | Status | Notes |
|---|---|---|---|
| `list_domains` | project | planned | Semantic grouping over `list_toolsets` |
| `describe_domain` | project | planned | |
| `describe_action` | project | planned | Carries the real `specification` schema — **may be load-bearing** if RB-03 q6 shows UHT generates a useless schema for `FString` parameters |
| `get_schema` | project | planned | |
| `get_examples` | project | planned | |
| `get_project_capabilities` | project | planned | |
| `get_job_result` | project | planned | Long-running job polling (ADR-0009, RB-04) |

## Actions

Nothing is `available` yet — Phase 0. Statuses below reflect the design, and the briefs
that must land first.

### Graph operations — the core thesis

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `get_asset_graph` | blueprints | WS-06 | research | RB-05 |
| `replace_blueprint_graph` | blueprints | WS-06 | research | RB-05 |
| `modify_blueprint_graph` | blueprints | WS-06 | research | RB-05 |
| `analyze_blueprint` | blueprints | WS-06 | planned | RB-05 |
| `repair_blueprint` | blueprints | WS-06 | planned | RB-05 |
| `create_blueprint` | blueprints | WS-06 | planned | |

### Batch and validation

| Action | Domain | WS | Status |
|---|---|---|---|
| `execute_plan` | project | WS-05 | planned |
| `validate_asset` | validation | WS-11 | planned |
| `validate_system` | validation | WS-11 | planned |

### Niagara

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_niagara_effect` | niagara | WS-07 | research | RB-07 |
| `create_niagara_template` | niagara | WS-07 | research | RB-07, RB-10 |
| `create_effect_variation` | niagara | WS-07 | research | RB-07, RB-10 |

### Materials and VFX assets

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_vfx_material` | materials | WS-08 | research | RB-08 |
| `create_material` | materials | WS-08 | planned | RB-08 |
| `create_material_family` | materials | WS-08 | planned | RB-08 |
| `import_and_configure_asset` | import_export | WS-08 | planned | RB-11 |

### Gameplay and GAS

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_spell` | gameplay | WS-09 | planned | RB-12 |
| `create_gameplay_ability` | gameplay_abilities | WS-09 | research | RB-12 |
| `create_gameplay_effect` | gameplay_abilities | WS-09 | research | RB-12 |
| `create_player_ability` | gameplay | WS-09 | planned | RB-12 |
| `create_gameplay_system` | gameplay | WS-09 | planned | RB-12 |
| `configure_replication` | networking | WS-09 | planned | RB-12 |
| `create_actor` / `create_component` | assets | WS-09 | planned | |
| `create_data_asset` | data_assets | WS-09 | planned | |

### Animation

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_animation_blueprint` | animation | WS-10 | research | RB-09 |
| `create_control_rig` | control_rig | WS-10 | research | RB-09 — **may prove read-only** |

### Templates

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `search_templates` | project | WS-15 | planned | RB-10 |
| `instantiate_template` | project | WS-15 | planned | RB-10 |
| `promote_to_template` | project | WS-15 | planned | RB-10 |

### Later domains

Designed for, not scheduled. Adding these must require **no protocol change** — if one
does, the architecture failed (`docs/ROADMAP.md`).

`create_behavior_tree` · `create_state_tree` · `create_pcg_system` ·
`create_level_sequence` · world building · level design · UI · audio · source control

## Reference toolset

| Action | Status | Notes |
|---|---|---|
| `ping` | scaffold | Uncompiled. RB-03 makes it real. |
| `echo` | scaffold | Uncompiled. Proves the ADR-0003 envelope contract. |
