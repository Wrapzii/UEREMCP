# Capability Catalog

**Owner:** WS-01. Workstreams register actions via `docs/proposals/`.

The registry of every agent-facing `action`. An action that is not listed here does not
exist as far as agents are concerned.

**Freshness:** 2026-07-30 — full-use integration. Overall POC A–E claimed; D5
multi-client, B10 rendered pixels, fail-soft Niagara inspect, and deterministic
visual capture are live-verified. **Not production-perfect** — Epic MCP cancel
adapter, durable-idempotency crash/migration, broad graph-fidelity, and metrics
caveats remain.

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
| `get_job_result` | project | available | Registered on `UeremcpReferenceToolset`; ADR-0009 process-local poll path. Timeout behaviour remains domain-adoption dependent. |
| `cancel_job` | project | available | Cooperative cancel for jobs that advertise `cancellable: true`; editor-verified via `UEREMCP.Transport.JobRegistry.Cancel`. **Not** Epic MCP `notifications/cancelled` — ToolsetRegistry adapter has no `CancelAsync` override (immutable UE 5.8 limitation). |

## Agent routing (prefer UEREMCP)

Canonical policy: [`docs/guide/tool-selection-policy.md`](guide/tool-selection-policy.md) /
[`docs/guide/tool-selection-contract.json`](guide/tool-selection-contract.json).

| Intent | Prefer (MCP tool) | Action | Avoid |
|---|---|---|---|
| Blueprint complete read | `UeremcpBlueprint…ReadGraph` | `read_graph` | BlueprintTools pin loops |
| Blueprint replace | `…SubmitGraph` | `submit_graph` | `create_node` / `write_graph_dsl` |
| Niagara create | `UeremcpNiagara…CreateNiagaraEffect` | `create_niagara_effect` | NiagaraToolsets primitives |
| Niagara inspect | `…InspectSystem` | `inspect_system` | ad-hoc topology scrapes |
| VFX material | `UeremcpMaterial…CreateVfxMaterial` | `create_vfx_material` | MaterialTools expressions |
| Template instantiate | `UeremcpTemplates…InstantiateTemplate` | `instantiate_template` | inventing ExecutePlan as first choice |
| Job poll/cancel | `UeremcpReference…GetJobResult` / `CancelJob` | `get_job_result` / `cancel_job` | MCP cancel alone |
| Visual frames (Niagara) | `UeremcpValidation…CaptureEffectFrames` | `capture_effect_frames` | screenshot authoring |
| World / material / anim frames | `…CaptureWorldFrames` / `CaptureMaterialFrames` / `CaptureAnimationFrames` | `capture_*_frames` | HighResShot-only |
| Mountain/river/forest/rain | `UeremcpEnvironment…BuildEnvironment` | `build_environment` | Landscape/Water primitive chains |
| Environment inspect/validate | `…InspectEnvironment` / `ValidateEnvironment` | `inspect_environment` / `validate_environment` | screenshot-as-gate |
| SoundCue + attenuation | `UeremcpSystems…CreateAudioCue` | `create_audio_cue` | MetaSound inventing |
| Replication / Pattern B | `…ValidateReplication` | `validate_replication` | `get/set_variable_replication` loops |
| World Partition | `…InspectWorldPartition` / `RepairWorldPartition` | `inspect_world_partition` / `repair_world_partition` | silent WP commandlets; HLOD claims |

Epic tools remain appropriate for read-only discovery and catalog gaps
(`planned` / `research`). This table does **not** claim arbitrary LLM tool choice
is guaranteed.

## Actions

Statuses below reflect **registered code + runtime/editor evidence**, not Phase 0 design
intent. Many surfaces remain `partial` because metrics (R-17), production visual
perfection beyond the B10 warm-pixel gate, Animation authoring, and domain-wide
ADR-0005 coverage outside proven scopes are still incomplete.

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
| `execute_plan` | project | WS-05 / WS-03 | partial | Agent-facing `UUeremcpReferenceToolset::ExecutePlan` (`AICallable`) delegates to `FUeremcpPlanActions` / `FUeremcpPlanExecutor`. Durable Claim/Complete idempotency under `Saved/UEREMCP/idempotency` (fingerprint-bound; restart replay verified). Still partial: metadata+package are not one atomic transaction; crash-after-mutation leaves a reclaimable in-progress claim; legacy `Put`/`TryGetReplay` call sites lack fingerprint conflict detection until migrated. |
| `validate_asset` | validation | WS-11 | planned | |
| `validate_system` | validation | WS-11 | planned | |
| `capture_effect_frames` | validation | WS-11 | available | `UeremcpValidation.UeremcpVisualCaptureToolset.CaptureEffectFrames`; warm path live-verified. Cold renderer residual: first call may return ADR-0009 `partially_completed`; poll `get_job_result` (non-cancellable). Fresh-editor cold path completed after one poll with changed lit pixels. Requires editor world + renderer/RHI; pixel change proves rendering, not appearance or structural correctness. |
| `capture_world_frames` | validation | WS-11 | partial | Editor-world SceneCapture2D + structural snapshot; beauty not gated; live RE pending on general-capture tip |
| `capture_material_frames` | validation | WS-11 | partial | Disposable lit stage + structural material identity; live pending |
| `capture_animation_frames` | validation | WS-11 | partial | Posed AnimSequence capture; needs `skeletal_mesh_path` when sequence has no mesh; live pending |

### Environment / world building

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `build_environment` | environment | WS-01 | partial | Seeded landscape + river + forest banks + rain (+ optional capture). Dry-run default. Screenshots never the structural gate. |
| `create_landscape` | environment | WS-01 | partial | Heightmap import via `ALandscape::Import`; ExecutePlan terrain stage |
| `create_water_body` | environment | WS-01 | partial | WaterBody river/lake/ocean from spline |
| `scatter_foliage` | environment | WS-01 | partial | Seeded HISM with river exclusion corridor |
| `attach_weather` | environment | WS-01 | partial | Rain/fog attach; PIE camera-follow proof still limited |
| `place_structures` | environment | WS-01 | partial | GeometryScript AppendBox along spline |
| `inspect_environment` | environment | WS-01 | partial | Read-only environment metrics |
| `validate_environment` | environment | WS-01 | partial | Structural gates only |

### Niagara

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `inspect_system` | niagara | WS-07 | partial | RB-07 — AICallable registered; editor Inspect filter PASS on recorded tips; topology intentionally lossy |
| `create_niagara_effect` | niagara | WS-07 | partial | RB-07 — POC B structural create live; ADR-0006 repeated-create + stale `expected_revision` gated (`UEREMCP.Validation.Domain.Niagara.*`); MutatingDispatch wired. B10 rendered warm-pixel / particle gate PASS via `UEREMCP.Niagara.POCB.VisibleRender` — do not equate that with production visual perfection across all scenes/hardware. |
| `create_niagara_template` | niagara | WS-07 | partial | POC C variation path landed via create + Templates; see POC C claim docs |
| `create_effect_variation` | niagara | WS-07 | partial | POC C ice/wind variation + C7 third generation claimed |

### Materials and VFX assets

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_vfx_material` | materials | WS-08 | partial | RB-08 — AICallable; MutatingDispatch wired; ADR-0006 repeated-create + stale `expected_revision` gated (`UEREMCP.Validation.Domain.Material.*`) |
| `create_procedural_texture` | materials | WS-08 | partial | AICallable registered; test-root `/Game/__UeremcpTests/`; MutatingDispatch wired |
| `create_material` | materials | WS-08 | planned | RB-08 |
| `create_material_family` | materials | WS-08 | planned | RB-08 |
| `import_and_configure_asset` | import_export | WS-08 | planned | RB-11 |

### Gameplay and GAS

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_spell` | gameplay | WS-09 | partial | RB-12 — live `execute_plan` / upsert under `/Game/__UeremcpTests/`; POC D MET; D5 static Pattern B minimum plus live multi-client listen-server proof via `tests/run_d5_multiclient.ps1` (`UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE`) |
| `create_gameplay_ability` | gameplay_abilities | WS-09 | research | RB-12 — textbook GAS out of RE POC D scope |
| `create_gameplay_effect` | gameplay_abilities | WS-09 | research | RB-12 |
| `create_player_ability` | gameplay | WS-09 | planned | RB-12 |
| `create_gameplay_system` | gameplay | WS-09 | planned | RB-12 |
| `configure_replication` | networking | WS-09 | superseded | Prefer WS-01 `validate_replication` (`UeremcpSystems`) for goal-level audit/configure; multi-client remains WS-11 |
| `create_actor` / `create_component` | assets | WS-09 | planned | |
| `create_data_asset` | data_assets | WS-09 | planned | |

### Audio / networking / world partition (remaining coverage)

| Action | Domain | WS | Status | Brief |
|---|---|---|---|---|
| `create_audio_cue` | audio | WS-01 | partial | SoundCue + optional attenuation via factories; MetaSound graph blocked; live handoff pending |
| `inspect_audio` | audio | WS-01 | partial | SoundCue/Wave/Attenuation re-read |
| `validate_replication` | networking | WS-01 | partial | Pattern B + Blueprint variable audit/configure in one call; no multi-client claim |
| `inspect_world_partition` | world_partition | WS-01 | partial | Read-only WP/streaming state |
| `repair_world_partition` | world_partition | WS-01 | partial | `CreateOrRepairWorldPartition`; dry_run default; HLOD builders blocked |

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
| `instantiate_template` | project | WS-15 | partial | RB-10 — materializes via internal `execute_plan`; POC C variation + C7 claimed — not a general template marketplace |
| `promote_to_template` | project | WS-15 | partial | RB-10 — preview-oriented until cross-domain gates bound |

### Later domains

Designed for, not scheduled. Adding these must require **no protocol change** — if one
does, the architecture failed (`docs/ROADMAP.md`).

`create_behavior_tree` · `create_state_tree` · `create_pcg_system` ·
`create_level_sequence` · UI · source control · MetaSound graph authoring · WP HLOD builders

## Reference toolset

| Action | Status | Notes |
|---|---|---|
| `get_started` | available | START HERE bootstrap; `UeremcpReferenceToolset.GetStarted` |
| `resolve_intent` | available | Plain-text → ordered live-registry plan; cannot emit absent names; abstains on hash mismatch / weak signal. Routing ≠ E2E success. |
| `describe_operation` | available | One registry-verified tool schema + example |
| `ping` | available | `UeremcpReferenceToolset` + domain pings; ADR-0002 reachability |
| `echo` | available | Envelope round-trip (ADR-0003); registered on Reference / domain toolsets |
