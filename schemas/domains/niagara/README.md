# Niagara domain schemas and graph mapping (WS-07)

**Owner:** WS-07. **ADR:** 0004 (no fork — stacks map to shared `graph.schema.json`).

## Actions

| Action | Specification schema | Status |
|---|---|---|
| `inspect_system` | `inspect_system.schema.json` | topology read via UNiagaraExternalEditUtilities |
| `create_niagara_effect` | `create_niagara_effect.schema.json` | POC B probe compose (template + emitters + User.*) |

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
| Event handler stacks (topology gap) | `extensions.niagara.event_handlers[]` (inferred placeholders from `GetStackIssues` / compile `per_script`; modules empty) |
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
| Event handler stacks omitted from `GetEmitterTopology` | High | `extensions.niagara.event_handlers[]` inferred placeholders; modules remain lossy |
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
