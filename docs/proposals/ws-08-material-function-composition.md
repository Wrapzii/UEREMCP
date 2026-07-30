# WS-08: MaterialFunction composition — research and implementation proposal

- **From:** WS-08
- **Date:** 2026-07-30
- **Status:** proposal — **not implemented** in `feature_graph_v1`
- **Supersedes:** partial notes in `RB-08-materials-and-textures.md` §A4 / §B7

## Problem

`UeremcpMaterialFeatureGraph` wires raw `MaterialExpression` nodes. That works for POC
projectile masters but duplicates subgraphs Epic already ships as MaterialFunctions
(Fresnel, panning, SubUV helpers, depth fade, etc.). Capability note
`material_function_internals` marks nested MF graphs as a lossy area
(`UeremcpMaterialCapabilityNotes.h`).

Goal-level material creation should **compose engine or project MaterialFunctions** where
they exist, and only fall back to expression wiring when audit shows no suitable MF.

## Verified UE 5.8 API surface

All claims below were read from headers on this machine (UE 5.8 install).

| Operation | API | Tag |
|---|---|---|
| MaterialFunction asset type | `UMaterialFunction` | `[VERIFIED: MaterialFunction.h:38-39]` |
| Function I/O boundary nodes | `UMaterialExpressionFunctionInput` / `UMaterialExpressionFunctionOutput` | `[VERIFIED: MaterialExpressionFunctionInput.h:46-47]`, `[VERIFIED: MaterialExpressionFunctionOutput.h:15-16]` |
| Call MF from material graph | `UMaterialExpressionMaterialFunctionCall::MaterialFunction` + `SetMaterialFunction` | `[VERIFIED: MaterialExpressionMaterialFunctionCall.h:84-86,157-158]` |
| Refresh call node I/O after MF edit | `UMaterialExpressionMaterialFunctionCall::UpdateFromFunctionResource` | `[VERIFIED: MaterialExpressionMaterialFunctionCall.h:161-164]` |
| Create MF asset | `UMaterialFunctionFactoryNew::FactoryCreateNew` | `[VERIFIED: MaterialFunctionFactoryNew.h:14-20]` |
| Add expression inside MF | `UMaterialEditingLibrary::CreateMaterialExpressionInFunction` | `[VERIFIED: MaterialEditingLibrary.h:367-373]` |
| Add expression on material (incl. FunctionCall) | `UMaterialEditingLibrary::CreateMaterialExpression` | `[VERIFIED: MaterialEditingLibrary.h:167-168]` |
| Wire expressions | `UMaterialEditingLibrary::ConnectMaterialExpressions` | `[VERIFIED: MaterialEditingLibrary.h:234-239]` |
| Recompile MF + dependent materials | `UMaterialEditingLibrary::UpdateMaterialFunction` | `[VERIFIED: MaterialEditingLibrary.h:383-388]` |
| List MF expressions | `UMaterialEditingLibrary::GetMaterialFunctionExpressions` | `[VERIFIED: MaterialEditingLibrary.h:137]` |
| Find materials using MF | `UMaterialEditingLibrary::GetMaterialsReferencingFunction` | `[VERIFIED: MaterialEditingLibrary.h:498]` |

Epic `MaterialTools` toolset (27 domain toolsets) exposes `create_function`, `add_expression`,
`recompile`, and `get_referencing_materials` for MaterialFunction workflows
(`docs/audit/raw/schemas/editor_toolset.toolsets.material.MaterialTools.json`).
**Audit before build:** prefer composing existing Epic MFs over new expression subgraphs.

## What is NOT verified on this machine

- Runtime AssetRegistry enumeration of `/Engine/Functions/**` MF names (filesystem scan
  unreliable per `RB-08` §B7). WS-02 / WS-11 editor harness must populate
  `docs/audit/` with a live inventory before hard-coding MF paths.
- Whether specific §6 feature tokens map 1:1 to a single engine MF (many are expression
  patterns, not packaged functions).

## Proposed design (Wave 3 slice)

### 1. Feature token → composition strategy table

Extend `UeremcpMaterialFeatures` with a resolution pass:

| Strategy | When | Action |
|---|---|---|
| `engine_material_function` | AssetRegistry finds MF under `/Engine/Functions` matching feature | `MaterialFunctionCall` + connect outputs |
| `project_material_function` | `/Game/__UeremcpTests/Materials/Functions/MF_Ueremcp_*` exists | Same |
| `expression_fallback` | No MF or MF I/O mismatch | Current `UeremcpMaterialFeatureGraph` path |
| `unimplemented` | No MF and no fallback | Surface in `capability_notes`; `partially_completed` on modify |

Store mapping in `schemas/domains/materials/feature_composition.v1.json` (new, WS-08 owned)
— **not** in envelope or graph schema.

### 2. New internal module (owned paths)

```
Plugins/UEREMCP/Source/UeremcpMaterial/
  Private/UeremcpMaterialFunctionComposer.cpp   // ensure MF, FunctionCall, UpdateFromFunctionResource
  Public/UeremcpMaterialFunctionComposer.h
```

`UeremcpMaterialFeatureGraph::Build` calls composer first per feature token; expression
builder remains fallback.

### 3. Verification bar

`VerifyFeatureGraph` gains optional check: when strategy is `engine_material_function`,
assert `GetAllExpressionsOfType<UMaterialExpressionMaterialFunctionCall>` count > 0 and
named MF asset loads.

Status remains honest: `created_and_validated` only when compile + re-read + feature
strategy satisfied.

### 4. Test plan

| Layer | Test |
|---|---|
| Offline | `test_material_function_composition.py` — proposal + composer stubs cited |
| Editor | `UeremcpMaterial.Toolset.CreateVfxMaterial.MaterialFunctionCompose` (after composer lands) |
| Audit | WS-02 row: Epic MF equivalents per WS-08 feature token |

## Phased delivery

1. **Phase A (this proposal):** document APIs, add `feature_composition.v1.json` skeleton,
   offline guards — **no runtime composer**.
2. **Phase B:** AssetRegistry probe tool (editor-only) listing candidate MFs; fill audit row.
3. **Phase C:** `UeremcpMaterialFunctionComposer` for 2–3 tokens (e.g. fresnel, depth_fade)
   with expression fallback.
4. **Phase D:** Replace expression fallbacks as audit confirms engine MF paths.

## Handoffs

| To | Need |
|---|---|
| WS-02 | `docs/audit/` row: MaterialTools MF tools vs UEREMCP composer; `/Engine/Functions` inventory from RE editor |
| WS-11 | Editor test filter `UeremcpMaterial.Toolset.*` after Phase C |
| WS-01 | No schema envelope change; approve `feature_composition.v1.json` domain file if shared catalog needed |

## Current WS-08 status

- `feature_graph_v1`: expression-only wiring for all implemented tokens including
  `distortion` and `flipbook_subuv` graph hooks.
- MaterialFunction composition: **not implemented** — this document is the handoff artifact.
