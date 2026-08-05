# Niagara domain schemas and graph mapping (WS-07)

**Owner:** WS-07. **ADR:** 0004 (no fork — stacks map to shared `graph.schema.json`).

## Actions

| Action | Specification schema | Status |
|---|---|---|
| `inspect_system` | `inspect_system.schema.json` | topology_summary at top + graphs[]; response_detail summary\|complete |
| `create_niagara_effect` | `create_niagara_effect.schema.json` | PRIMARY: emitters[].modules[{primitive_id\|asset_path,inputs}] on Minimal substrate |
| `submit_niagara_graph` | `submit_niagara_graph.schema.json` | graphs[] and/or emitters[].modules[]; Minimal when adding |
| `adapt_niagara_effect` | `adapt_niagara_effect.schema.json` | User.* + materials + emitters[{sim_target,life_cycle}] |

Register `inspect_system` in `docs/CAPABILITY_CATALOG.md` via proposal to WS-01 when the
tool leaves scaffold status.

## Module stacks → `graph.schema.json`

Niagara is not one graph. A **system** decomposes into nested graphs per ADR-0004
`graph_type` discriminator:

```
NiagaraSystemGraph
├── subgraphs[] → NiagaraEmitterGraph (per emitter)
│   ├── subgraphs[] → NiagaraModuleStack (per script usage)
│   │   ├── nodes[]   modules in execution order (from GetEmitterTopology / GetScriptStackTopology)
│   │   ├── links[]   optional sequential exec edges (kind: exec) for agents that prefer edge lists
│   │   └── extensions.niagara  script_usage, inputs{}, module metadata
│   ├── nodes[]       renderer summary nodes (optional)
│   └── extensions.niagara.emitter  GetEmitterData.propertyValues blob
├── variables[]       system user parameters (User.*)
└── extensions.niagara.system / compile / user_parameters
```

### `graph_type` roles

| `graph_type` | Source API | `nodes[]` meaning |
|---|---|---|
| `NiagaraModuleStack` | `GetScriptStackTopology`, `GetModuleTopology` | One module per node; `semantic_id` = `{emitter}/{script_usage}/{module_name}` |
| `NiagaraEmitterGraph` | `GetEmitterTopology` | Subgraph refs + renderer nodes; settings in `extensions.niagara.emitter` |
| `NiagaraSystemGraph` | `GetSystemSummary`, `GetSystemDependencies` | Emitter subgraph refs; user vars in `variables[]` |
| `NiagaraScriptGraph` | Module/dynamic-input EdGraph | **Out of POC B/C scope** — composition of existing modules only |

### What stays in `extensions.niagara` (not fake `links`)

Per RB-07 R-05 verdict — do **not** invent data `links` between stack modules for
parameter bindings:

| Data | Location |
|---|---|
| Value modes (local, linked, DI, HLSL, dynamic chain) | `extensions.niagara.inputs[pin_id]` |
| Renderer `propertyValues` JSON | `extensions.niagara.renderers[]` (+ optional `material_path`, unvalidated) |
| Event handler stacks | `extensions.niagara.event_handlers[]` from `GetEventHandlers` (+ NodeGraph samples). WRITE blocked (no UsageId on `StackItemReference`) |
| SimTarget / Life Cycle | `extensions.niagara.sim_target` + `life_cycle{mode,loop_behavior,loop_duration,inactive_response}` |
| Script graph internals | READ summary via `UNiagaraScriptSource::NodeGraph`; WRITE unsupported |
| Inheritance metadata | `extensions.niagara.inheritance` |
| Compile aggregate + per-script | `extensions.niagara.compile` |

Schema: `graph-ext.schema.json`.

### Fidelity (honest defaults until round-trip proven)

Every retrieved graph includes:

```json
"fidelity": {
  "round_trip_supported": false,
  "lossy_areas": [
    "event_handler_stacks",
    "module_reorder_without_readd",
    "script_graph_internals"
  ]
}
```

These keys match `UeremcpNiagaraCapabilityNotes.h` and `inspect_system` `capability_notes`.

POC B multi-graph hash scaffold (`hash_round_trip_poc_b_scaffold.json`) and
`hash_round_trip_retrieve_submit.json` document the harness.
`EvaluateRetrieveSubmitRetrieveStability` flips `round_trip_supported` **only** when
pre/post submit hashes match on a live system; otherwise stays false with an explicit
`failure_mode`.

### POC B gate scaffolding (`extra.poc_b_gates`)

`FUeremcpNiagaraPocBGates` surfaces honest partial status for acceptance criteria B4/B7:

| Field | Meaning |
|---|---|
| `B4_material_bindings_verified` | All requested material roles passed renderer re-read |
| `B7_emitters_non_empty` | Create added at least one emitter |
| `B7_structural_match` | Post-create inspect structural match (when `options.validate`) |
| `B7_renderers_present` | Every created emitter has renderer refs on inspect (topology only) |
| `B7_renderers_bound` | **false** until `material_bindings.bAllRequestedVerified`; inspect `material_path` does not count |
| `B7_data_interfaces_complete` | **false** when dependencies observed; **null** without inspect dependencies — never inferred complete |
| `inspect_fidelity` | Observational counts (`used_data_interfaces`, extracted material paths) |
| `round_trip_supported` | always **false** until hash round-trip proven |

Response status remains **`partially_completed`**; `never_claims` lists `*_validated` statuses.

`create_niagara_effect` registers with `FUeremcpPlanExecutor` at Niagara module startup
(WS-15 handler contract). Dry-run plan steps surface `no_change_required`; mutating
create stays **`partially_completed`**. Atomic template plans still reject before
mutation until WS-03 transaction callbacks and WS-08 `create_vfx_material` land.

## Epic tool composition (implementation note)

UEREMCP does **not** re-expose NiagaraToolsets' 46 primitives. Internal batching via
`ProgrammaticToolset.execute_tool_script` (see `$RAT/Docs/NIAGARA_BATCHING.md`):

1. `GetSystemSummary` / `GetEmitterTopology` / input value getters
2. `GetSystemCompileState` (await — 120s default)
3. Map to graphs + `extensions.niagara`
4. Hide primitives with `SetNameFilters` (ADR-0002)

## Known gaps (capability_notes)

| Gap | Severity | Mitigation |
|---|---|---|
| Event handler stacks omitted from `GetEmitterTopology` | High | READ via `GetEventHandlers` + NodeGraph samples; WRITE blocked (no UsageId) |
| Renderer material paths from `GetRendererData` | Medium | `material_path` best-effort extract; `renderer_material_bindings` on emitter graphs |
| No `ReorderModule` AICallable | High | remove+re-add or internal `MoveModule` proposal to WS-03 |
| `CreateNiagaraSystem` requires template | By design | duplicate-and-modify |
| No headless particle sim | Medium | compile + structure + save; optional place |
| `NiagaraScriptGraph` authoring | Out of scope | compose Epic modules only |

Full research: `docs/research/RB-07-niagara.md`.

### Post-create inspect (`options.validate`)

When `create_niagara_effect` runs with `options.validate: true` (default), `FUeremcpNiagaraRoundTrip`
re-reads the saved asset via `inspect_system` and sets `validation.structurally_valid` when emitter
names and `User.*` parameters match. This is **not** ADR-0004 content-hash round-trip; status stays
`partially_completed`.

### Idempotent probes (`mode: replace`)

Use envelope **`mode: "replace"`** (not a specification field) to delete and recreate assets under
`/Game/__UeremcpTests/` only. Paths outside the probe root are rejected for deletion
(`UeremcpNiagaraProbeAssets::DeleteProbeAssetAtPath`). Destructive tier defaults to dry-run unless
`options.dry_run: false` is explicit (ADR-0010). Status remains **`partially_completed`**, never
`*_validated`.

Example target: `/Game/__UeremcpTests/NS_WS07_RoundTripProbe`.

### POC B six-emitter plan (`create_poc_b_six_emitter_plan.json`)

Elemental projectile acceptance target from `docs/POC_ACCEPTANCE.md` POC B — six component
roles composed from Epic default emitter templates:

| Role | Emitter name | Template |
|---|---|---|
| `core` | Core | `/Niagara/DefaultAssets/Templates/Emitters/Minimal` |
| `flame_shell` | FlameShell | `…/UpwardMeshBurst` |
| `sparks` | Sparks | `…/SimpleSpriteBurst` |
| `smoke` | Smoke | `…/Fountain` |
| `ribbon_trail` | RibbonTrail | `…/LocationBasedRibbon` |
| `impact_burst` | ImpactBurst | `…/OmnidirectionalBurst` |

System template default: `/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight`.

Implementation mapping lives in `UeremcpNiagaraRoles::DefaultPocBComponentRoles()` and
`ResolveEmitterTemplatePath()`. Offline fixture validates the request shape and documents
honest criterion status — response stays **`partially_completed`**, never `*_validated`,
until B4/B7/B10 gates pass in editor (WS-11).

Probe target for live runs: `/Game/__UeremcpTests/NS_POCB_FireballProbe`.

### B7 editor gate scaffold (WS-11 handoff)

Offline implementation maps inspect graphs to `extra.poc_b_gates` on mutating create
when `options.validate=true`. Editor proof is **not** done — WS-11 owns runtime
verification using `fixtures/poc_b_editor_gate_scaffold.json`.

| Check | Offline today | Editor proof |
|---|---|---|
| Emitters non-empty | `B7_emitters_non_empty` from create result | Re-read emitter count |
| Renderers present | `B7_renderers_present` from inspect topology | Inspect all six roles |
| Renderers bound | `B7_renderers_bound` false unless material re-read verified | B4 full bind proof |
| Data interfaces | `B7_data_interfaces_complete` always false when evaluated | Dependency audit only |
| Structural match | `B7_structural_match` when round-trip inspect ran | WS-11 structural spec |

Handoff detail: `docs/proposals/ws-07-b7-editor-gate-handoff.md`.

### Material bindings (`specification.materials`)

Direct probe material paths under `/Game/__UeremcpTests/Materials/` are assigned via
`GetRendererData` / `SetRendererData` on matching emitter renderers, with re-read
verification. Inline `{create_spec}` entries delegate to `UeremcpMaterialNiagaraExport`
(probe MI paths only; requires orch merge of WS-08 `UeremcpMaterial`). Orphan inline MIs
after bind failure surface as `orphaned_inline_creates`. Status stays
**`partially_completed`** unless every requested role re-reads equal — never
`*_validated` without full POC B gate.

### Content hash scaffolding (`FUeremcpNiagaraGraphHash` / `FUeremcpNiagaraHashRoundTrip`)

Every inspect graph receives `content_hash` + `revision` via `FUeremcpContentHash`
(ADR-0004 / WS-05). `fidelity.round_trip_supported` remains **false** until WS-11 proves
retrieve → replace → retrieve hash stability on probe assets. Response
`diagnostics.hash_scaffold` carries a graph_id → hash manifest only.

## Tests

Runtime probes and created assets: **`/Game/__UeremcpTests/`** only (e.g. `NS_WS07_Probe`).

```bash
python tools/validate_schemas.py
python schemas/domains/niagara/test_specifications.py
python tools/check_ownership.py --ws WS-07
```
