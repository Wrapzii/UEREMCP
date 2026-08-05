# Proposal: Material Inspect/Submit routing + GetStarted (WS-08 → WS-01 / WS-03 / WS-13)

**From:** WS-08  
**To:** WS-01 (`docs/CAPABILITY_CATALOG.md`), WS-03 (`UeremcpCore` IntentRouter — **user-directed edits applied**), WS-13 (`docs/guide/`)  
**Date:** 2026-08-05  
**Status:** Material graph inspect/submit shipped on `ws-08-material-graph-roundtrip`; Core routing + catalog edits applied per user direction.

## Aligns with

- `docs/proposals/ws-11-world-document-model.md` §5 `submit_material_graph` (material floor)
- `docs/proposals/ws-08-epic-material-audit.md` `retrieve_material_graph` / `replace_material_graph` → agent names **InspectMaterial** / **SubmitMaterialGraph**
- Niagara parity pattern (`result.graphs[]`, `round_trip_supported=false`, ExtraFields.result packaging)

## Shipped (WS-08 owned)

| Item | Status |
|---|---|
| `InspectMaterial` / `inspect_material` | shipped — path OR query under `/Game` |
| `SubmitMaterialGraph` / `submit_material_graph` | shipped — in-place; production never silent-deleted |
| Schemas `inspect_material` / `submit_material_graph` / `graph-ext` | shipped |
| Plan handlers | shipped |
| Toolset version `0.3.0-material-graph` | shipped |
| Editor tests `UEREMCP.Material.Inspect.*` / `Submit.*` | shipped |
| `operation_catalog.json` (Content + tools) | shipped |
| IntentRouter demote Niagara for material/Free_Spells | shipped (user-directed Core edit) |
| GetStarted `material_path_policy` | shipped (user-directed Core edit) |

## Path policy

| Op | Allowed paths |
|---|---|
| InspectMaterial | any `/Game/…` (production Free_Spells OK) |
| SubmitMaterialGraph in-place | existing `/Game/…` assets (MIC params / existing-node links) |
| Create / expression add-delete | scratch only: `/Game/__UeremcpTests`, `/Game/__UeremcpPoc` |
| `delete_missing_expressions` | scratch + `mode=replace` only; **not auto-executed** until round-trip proven |

## Lossy / honesty

`fidelity.round_trip_supported=false` with lossy areas:
`expression_subclass_properties`, `material_function_internals`, `editor_chrome`.

Statuses: inspect → `partially_completed`; submit → `no_change_required` / `partially_completed` / `failed_validation`. Never `*_validated` until hash proof.

## Ask WS-01

- Add `inspect_material` + `submit_material_graph` rows to `docs/CAPABILITY_CATALOG.md`.
- Optional envelope deep-merge of `ExtraFields.result` (same ask as WS-07 Niagara).

## Ask WS-13

- Document InspectMaterial → edit JSON → SubmitMaterialGraph loop; CaptureMaterialFrames for proof.
- Note Free_Spells / production inspect is first-class; do not route material inspect to Niagara InspectSystem.

## Example envelopes

```json
{
  "protocol_version": "1.0",
  "action": "inspect_material",
  "specification": { "query": "M_Free_Spells_Flash", "search_root": "/Game" }
}
```

```json
{
  "protocol_version": "1.0",
  "action": "submit_material_graph",
  "target": { "asset_path": "/Game/.../MI_Free_Spells_Flash2" },
  "options": { "dry_run": true },
  "specification": {
    "parameters": { "scalar": { "EmissiveScale": 4.0 } }
  }
}
```
