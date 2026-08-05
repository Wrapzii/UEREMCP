# Proposal: GetStarted + catalog for submit_niagara_graph (WS-07 → WS-01 / WS-03 / WS-13)

**From:** WS-07  
**To:** WS-01 (`docs/CAPABILITY_CATALOG.md`, envelope ExtraFields.result merge), WS-03 (`UeremcpCore` IntentRouter / GetStarted / operation_catalog), WS-13 (`docs/guide/`)  
**Date:** 2026-08-05  
**Status:** User-directed WS-03 GetStarted + catalog edits applied on `ws-07-submit-niagara-graph`; remaining WS-01/WS-13 asks below.

## Need

WS-07 shipped agent-facing **`SubmitNiagaraGraph` / `submit_niagara_graph`**: one WRITE that accepts edited `inspect_system` `graphs[]` and reconciles an existing Niagara system under sandbox or Magecraft. Inspect defaults to **complete** graphs unless explicitly `summary`/`minimal`.

Smoke also showed Inspect packing graphs under `diagnostics.graphs` because `SerializeResponse` builds a thin `result` from `PrimaryAsset` and then **skips** `ExtraFields.result` when the key already exists. Agents must not dig diagnostics for the primary payload.

## Done (user-directed / WS-07 owned)

1. **Inspect packaging:** `result.graphs[]` + summary fields (`primary_asset`, `asset_path`, emitters, user_parameters, fidelity). `diagnostics` keeps `execution_trace` / `hash_scaffold` only. Same ExtraFields.result survival pattern for Adapt/Submit.
2. **GetStarted `niagara_path_policy`:** READ any `/Game`; Create/Adapt/Submit under Magecraft + sandbox; replace-delete sandbox-only; Prefer InspectSystem → SubmitNiagaraGraph; Adapt for light tweaks; `round_trip_supported=false`.
3. **Capability notes / toolset comments / mcp-bootstrap:** aligned with `result.graphs[]` and honest `round_trip_supported=false`.
4. **operation_catalog `inspect_system`:** `example_response_shape` documents `result.graphs[]`.
5. **Inspect tests:** assert `result.graphs` presence (not diagnostics-only).

## Ask WS-03 (partially applied)

If another agent owns a later IntentRouter pass, keep `niagara_path_policy` as:

1. READ: `UeremcpNiagara.InspectSystem` — one-shot `result.graphs[]` (complete by default).
2. WRITE light: `AdaptNiagaraEffect` (User.* / materials).
3. WRITE full graph JSON: Prefer `InspectSystem` → `SubmitNiagaraGraph`.
4. WRITE new asset: `CreateNiagaraEffect`.
5. Magecraft: never delete UAsset; `mode=replace` on submit = in-place stack reconcile only; dry_run defaults for destructive modes (ADR-0010).
6. `fidelity.round_trip_supported=false` until hash stability proven.

Also register `submit_niagara_graph` / `adapt_niagara_effect` in `Plugins/UEREMCP/Content/IntentRouter/operation_catalog.json` and `tools/intent_router/operation_catalog.json` if those remain the live sources and entries are still missing.

## Ask WS-01

- Add `submit_niagara_graph` row to `docs/CAPABILITY_CATALOG.md`.
- Optional envelope improvement: deep-merge `ExtraFields.result` into the PrimaryAsset-built `result` instead of skipping the key — several domains hit this (Niagara Inspect/Adapt/Submit, UI). Workaround today: leave `Response.PrimaryAsset` empty and put `primary_asset` inside `ExtraFields.result`.
- Optional envelope codes: keep `NIAGARA_MUTATE_PATH_DENIED`; no new code required for submit if path policy reuses it.

## Ask WS-13

- Update `docs/guide/tool-selection-policy.md` / bootstrap copy: Inspect → edit graphs JSON → SubmitNiagaraGraph is the preferred Magecraft authoring loop for structural edits; Adapt for light tweaks.
- Note lossy areas: event_handler_stacks, module_reorder_without_readd, script_graph_internals, renderer_material_bindings.

## WS-07 status (owned paths)

| Item | Status |
|---|---|
| `schemas/domains/niagara/submit_niagara_graph.schema.json` | shipped |
| `UUeremcpNiagaraToolset::SubmitNiagaraGraph` | shipped (toolset 0.9.9-submit-graph) |
| Plan handler `submit_niagara_graph` | shipped |
| Inspect default complete + GetEmitterData | shipped |
| Inspect `result.graphs[]` packaging | **fixed** (was dropped vs diagnostics mirror) |
| `round_trip_supported` | **false** (honest) |
| Hash retrieve→replace→retrieve proof | **not** done — do not flip fidelity |

## Example envelopes

Inspect (complete by default) — expect `result.graphs[]`:

```json
{
  "protocol_version": "1.0",
  "action": "inspect_system",
  "target": {
    "asset_path": "/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"
  },
  "specification": {}
}
```

Submit dry_run (Magecraft, replace = in-place):

```json
{
  "protocol_version": "1.0",
  "action": "submit_niagara_graph",
  "mode": "replace",
  "target": {
    "asset_path": "/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"
  },
  "options": { "dry_run": true },
  "specification": {
    "graphs": []
  }
}
```

(`graphs` must be the edited inspect payload — empty array is schema-invalid; pass full inspect `result.graphs`.)
