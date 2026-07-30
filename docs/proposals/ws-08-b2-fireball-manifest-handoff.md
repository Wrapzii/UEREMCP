# WS-08 → WS-07: B2 material manifest handoff (POC B)

- **From:** WS-08
- **To:** WS-07, WS-01
- **Date:** 2026-07-30
- **Status:** WS-08 landed; WS-07 integration pending

## What B2 requires

POC B criterion B2 (`docs/POC_ACCEPTANCE.md`): required materials are created **or**
reused; reuse is reported in `result.reused_assets`.

Envelope contract (`schemas/envelope/response.schema.json`): `reused_assets` lists
existing assets the operation depended on but did not modify.

## What WS-08 landed

`create_vfx_material` and `create_procedural_texture` now populate
`result.reused_assets` on owned paths:

| Asset | First call | Second call (same purpose / path) |
|---|---|---|
| Feature master (`M_Ueremcp_*`) | `created_assets` (`role: master_template`) | `reused_assets` (`role: master_template`) |
| Target MI | `created_assets` (`role: <purpose>`) | `modified_assets` (parent/params updated) |
| Bound texture path (string slot) | — | `reused_assets` (`role: <slot name>`) |
| Nested procedural texture (idempotent) | `created_assets` | `reused_assets` |

Editor proof: `UeremcpMaterial.Toolset.CreateVfxMaterial.MasterReuseManifest`
(two MIs, one shared master — second response lists master in `reused_assets` only).

Toolset wiring: `Response.ReusedAssets = CreateResult.ReusedAssets`
`[VERIFIED: UeremcpMaterialToolset.cpp]`.

`execute_plan` already aggregates `reused_assets` across operations
`[VERIFIED: UeremcpPlanExecutor.cpp:702-715]`.

## What WS-07 must do (not WS-08)

For a single fireball `create_niagara_effect` response to satisfy B2:

1. For each `materials.<role>` inline `{create_spec, reuse_if_present}` entry, call
   `UeremcpMaterialService::ExecuteCreateVfxMaterial` (or plan step) with a
   deterministic MI path (see `docs/proposals/ws-07-niagara-material-bindings.md`).
2. **Merge** each material sub-result's `created_assets`, `modified_assets`, and
   `reused_assets` into the Niagara response `result` object — do not collapse to
   `dependencies` only.
3. When `reuse_if_present: true` and the MI already exists, load it and list the MI
   in `reused_assets` (WS-07-owned path resolution); WS-08 reports MI reuse only
   when the material service is invoked and performs an idempotent ensure.
4. Copy material `PrimaryAsset` canonical paths into renderer binding verification
   (B4/B7).

WS-08 will **not** edit `Plugins/UEREMCP/Source/UeremcpNiagara/**`.

## Remaining B2 gaps (honest)

| Gap | Owner |
|---|---|
| Single fireball MCP response aggregating all six role materials in one manifest | WS-07 |
| `reuse_if_present` short-circuit without re-invoking create when MI on disk | WS-07 |
| Cross-domain `execute_plan` / FileSandbox atomic manifest (B9 partial) | WS-01 + WS-07 |
| Overall POC-B claim | **Not met** — B2 material slice only |

## No claim

This handoff does **not** satisfy overall POC-B or B9. Material toolset 11/11 +
VisualTest T1a + this manifest work are **inputs** to B2, not proof of B2 inside
one fireball run.
