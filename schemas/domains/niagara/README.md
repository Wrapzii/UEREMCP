# Niagara domain schemas and graph mapping (WS-07)

**Owner:** WS-07. **ADR:** 0004 (no fork — stacks map to shared `graph.schema.json`).

## Actions

| Action | Specification schema | Status |
|---|---|---|
| `inspect_system` | `inspect_system.schema.json` | Wave 2 scaffold (stub) |
| `create_niagara_effect` | `create_niagara_effect.schema.json` | specification only |

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
| Renderer `propertyValues` JSON | `extensions.niagara.renderers[]` |
| Event handler stacks (topology gap) | `extensions.niagara.event_handlers[]` |
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
| Event handler stacks omitted from `GetEmitterTopology` | High | `extensions.niagara.event_handlers`; document as lossy |
| No `ReorderModule` AICallable | High | remove+re-add or internal `MoveModule` proposal to WS-03 |
| `CreateNiagaraSystem` requires template | By design | duplicate-and-modify |
| No headless particle sim | Medium | compile + structure + save; optional place |
| `NiagaraScriptGraph` authoring | Out of scope | compose Epic modules only |

Full research: `docs/research/RB-07-niagara.md`.

## Tests

Runtime probes and created assets: **`/Game/__UeremcpTests/`** only (e.g. `NS_WS07_Probe`).

```bash
python tools/validate_schemas.py
python schemas/domains/niagara/test_specifications.py
python tools/check_ownership.py --ws WS-07
```
