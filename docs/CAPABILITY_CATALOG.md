# Capability Catalog

**Owner:** WS-01. Workstreams register actions via `docs/proposals/`.

The registry of every agent-facing `action`. An action that is not listed here does not
exist as far as agents are concerned.

**Freshness:** 2026-07-30 — statuses refreshed from registered `AICallable` toolsets and
the acceptance-gap audit (`docs/proposals/ws-01-acceptance-gap-audit-2026-07-30.md`).
**No overall POC-B claim.** Do not invent `available` without tests + verification.

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
| `get_job_result` | project | partial | Registered on `UeremcpReferenceToolset`; ADR-0009 poll path. Transport timeout/cancel SKIP residuals remain (audit P1). |
| `cancel_job` | project | partial | Registered on `UeremcpReferenceToolset`; cooperative cancel. Same ADR-0009 residuals. |

## Actions

Statuses below reflect **registered code + runtime/editor evidence**, not Phase 0 design
intent. Many surfaces remain `partial` because POC-B visibility, metrics, or domain-wide
ADR-0005/0006/0010 proofs are incomplete.

### Graph operations — the core thesis

MCP tools: `ReadGraph` / `SubmitGraph` (`action=read_graph` / `submit_graph`). Catalog
names below are the semantic actions; notes map to the live tool surface.

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `get_asset_graph` / `read_graph` | blueprints | WS-06 | partial | RB-05 — scoped CRT A1–A11 PASS on `3756244`; not arbitrary complex graphs; `tokens_total=0` on recorded runs |
| `replace_blueprint_graph` / `submit_graph` (`mode: replace`) | blueprints | WS-06 | partial | RB-05 — scoped replace validated in CRT; unchanged replace → `no_change_required` |
| `modify_blueprint_graph` (`mode: patch`) | blueprints | WS-06 | planned | Patch mode unimplemented (A8 escape hatch) |
| `analyze_blueprint` | blueprints | WS-06 | planned | RB-05 |
| `repair_blueprint` | blueprints | WS-06 | planned | RB-05 — out of POC A/B critical path |
| `create_blueprint` | blueprints | WS-06 | planned | |

### Batch and validation

| Action | Domain | WS | Status | Notes |
|---|---|---|---|---|
| `execute_plan` | project | WS-05 / WS-03 | partial | Agent-facing `UUeremcpReferenceToolset::ExecutePlan` (`AICallable`) delegates to `FUeremcpPlanActions` / `FUeremcpPlanExecutor` (`fc98fbc` / `bd9b2ba`). Templates also bind the executor internally. Still partial: RE/MCP smoke and real domain-handler plan run not yet recorded on this tip. |
| `validate_asset` | validation | WS-11 | planned | |
| `validate_system` | validation | WS-11 | planned | |

### Niagara

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `inspect_system` | niagara | WS-07 | partial | RB-07 — AICallable registered; editor Inspect filter PASS on recorded tips; topology intentionally lossy |
| `create_niagara_effect` | niagara | WS-07 | partial | RB-07 — MCP B1/B6 structural PASS; editor B2–B9 structural PASS; B10 production FAIL (0 warm pixels); create returns `partially_completed` until B10+metrics close. **No overall POC-B.** |
| `create_niagara_template` | niagara | WS-07 | research | RB-07, RB-10 — POC C not started |
| `create_effect_variation` | niagara | WS-07 | research | RB-07, RB-10 — POC C not started |

### Materials and VFX assets

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_vfx_material` | materials | WS-08 | partial | RB-08 — AICallable; Material Toolset editor PASS 14/14 on `d691316`; POC-B material reuse is structural — visibility ownership stays with WS-07 unless materials prove invisible |
| `create_procedural_texture` | materials | WS-08 | partial | AICallable registered; test-root `/Game/__UeremcpTests/` |
| `create_material` | materials | WS-08 | planned | RB-08 |
| `create_material_family` | materials | WS-08 | planned | RB-08 |
| `import_and_configure_asset` | import_export | WS-08 | planned | RB-11 |

### Gameplay and GAS

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_spell` | gameplay | WS-09 | partial | RB-12 — AICallable preflight only; **no asset mutation**; POC D not started. Do not read as a finished spell pipeline. |
| `create_gameplay_ability` | gameplay_abilities | WS-09 | research | RB-12 — textbook GAS out of RE POC D scope |
| `create_gameplay_effect` | gameplay_abilities | WS-09 | research | RB-12 |
| `create_player_ability` | gameplay | WS-09 | planned | RB-12 |
| `create_gameplay_system` | gameplay | WS-09 | planned | RB-12 |
| `configure_replication` | networking | WS-09 | planned | RB-12 |
| `create_actor` / `create_component` | assets | WS-09 | planned | |
| `create_data_asset` | data_assets | WS-09 | planned | |

### Animation

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `inspect_montage` | animation | WS-10 | partial | RB-09 — AICallable; Animation filter PASS 10/10 on `5ea9277`; complete `asset_state` withheld pending ADR-0011 |
| `read_anim_bp` | animation | WS-10 | partial | AICallable read-only AnimBP graph retrieval; authoring unsupported |
| `create_animation_blueprint` | animation | WS-10 | research | RB-09 |
| `create_control_rig` | control_rig | WS-10 | research | RB-09 — **may prove read-only** |

### Templates

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `search_templates` | project | WS-15 | partial | RB-10 — AICallable; Templates Toolset PASS 4/4 on `f15ea96` |
| `instantiate_template` | project | WS-15 | partial | RB-10 — materializes via internal `execute_plan`; POC C not started |
| `promote_to_template` | project | WS-15 | partial | RB-10 — preview-oriented until cross-domain gates bound |

### Later domains

Designed for, not scheduled. Adding these must require **no protocol change** — if one
does, the architecture failed (`docs/ROADMAP.md`).

`create_behavior_tree` · `create_state_tree` · `create_pcg_system` ·
`create_level_sequence` · world building · level design · UI · audio · source control

## Reference toolset

| Action | Status | Notes |
|---|---|---|
| `ping` | available | `UeremcpReferenceToolset` + domain pings; ADR-0002 reachability |
| `echo` | available | Envelope round-trip (ADR-0003); registered on Reference / domain toolsets |
