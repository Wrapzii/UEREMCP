# Materials domain schemas (WS-08)

**Owner:** WS-08. **ADR:** 0004 (material expression graphs map to shared `graph.schema.json` with extensions — proposal pending WS-01).

## Actions

| Action | Specification schema | Status |
|---|---|---|
| `create_vfx_material` | `create_vfx_material.schema.json` | Wave 2 slice — elemental projectile core/trail wired via MaterialEditingLibrary |
| `create_procedural_texture` | `create_procedural_texture.schema.json` | Wave 2 slice — CPU pixel fill via FImageUtils::CreateTexture2D; also invoked from `textures.generate` slots |

Register `create_vfx_material` in `docs/CAPABILITY_CATALOG.md` via proposal to WS-01 when the tool leaves scaffold status.

## Element-parameter model (ADR-0008)

Elemental VFX materials use **one tooling surface** (`create_vfx_material`) with parameterized templates — not per-element tools.

| Layer | Owner | Content |
|---|---|---|
| Master materials | WS-08 | Blend/domain/usage; shared MF wiring (6 masters — RB-08 §B8) |
| Element templates | WS-15 + WS-08 | `element` → default parameter map + texture slots (`templates/elements/`) |
| Effect instances | Agent request | `create_vfx_material` with `element` + `purpose` + overrides |
| Niagara binding | WS-07 | Renderer material = MI path; same parameter names (`ParticleColor`, …) |

### WS-15 elemental projectile alignment

`templates/niagara/niagara.projectile.elemental.v1.json` construction_plan delegates to `create_vfx_material` with:

| Plan step | `purpose` | `element` | Typical `features` |
|---|---|---|---|
| `core_material` | `elemental_projectile_core` | `{{inputs.element}}` | radial_falloff, animated_noise, fresnel, dynamic_color, dynamic_intensity |
| `trail_material` | `elemental_projectile_trail` | `{{inputs.element}}` | panning_textures, erosion, depth_fade, dynamic_color |

`modifiers` on this schema match `supported_modifiers` on that template (`crystalline_fragments`, `reduce_trail_persistence`, `boost_impact`, `preserve_networking`).

### Shared semantic parameters (cross-element)

| Parameter | Type | Role |
|---|---|---|
| `ParticleColor` | Vector | Niagara convention; primary tint |
| `ColorSecondary` | Vector | Gradient / core-to-edge |
| `EmissiveScale` | Scalar | HDR intensity |
| `SoftEdge` | Scalar | Radial falloff |
| `DepthFade` | Scalar | Camera fade distance |
| `DistortionStrength` | Scalar | Refraction/normal offset |
| `FlowSpeed` | Scalar | Pan / flow map speed |
| `Turbulence` | Scalar | Noise scale/chaos |
| `DissolveAmount` | Scalar | Erosion/dissolve |

### Feature tokens → master graph wiring (Wave 2 slice)

| Feature | Expression pattern | Verification |
|---|---|---|
| `dynamic_color` | Lerp(`ParticleColor`, `ColorSecondary`) | `[VERIFIED: MaterialExpressionLinearInterpolate.h]` |
| `dynamic_intensity` | Multiply × `EmissiveScale` → `MP_EmissiveColor` | `[VERIFIED: MaterialEditingLibrary.h:232]` |
| `radial_falloff` | `SphereMask` × `TextureCoordinate`, `SoftEdge` → hardness | `[VERIFIED: MaterialExpressionSphereMask.h]` |
| `animated_noise` | Time + Panner → `Noise` × emissive, `Turbulence` → filter width | `[VERIFIED: MaterialExpressionNoise.h]` |
| `fresnel` | `Fresnel` × emissive | `[VERIFIED: MaterialExpressionFresnel.h]` |
| `panning_textures` | Panner(`FlowSpeed`) × `MainTexture` sample → emissive | `[VERIFIED: MaterialExpressionPanner.h]`, `[VERIFIED: MaterialExpressionTextureSampleParameter2D.h]` |
| `depth_fade` | `DepthFade`(`DepthFade` param) → `MP_Opacity` | `[VERIFIED: MaterialExpressionDepthFade.h]` |
| `erosion` | `OneMinus`(`DissolveAmount`) × opacity | `[VERIFIED: MaterialExpressionOneMinus.h]` |
| `flow_maps` | Panner(`FlowSpeed`) × `FlowMap` sample → emissive | `[VERIFIED: MaterialExpressionPanner.h]`, `[VERIFIED: MaterialExpressionTextureSampleParameter2D.h]` |
| `distortion` | `BumpOffset`(`NoiseTexture`, `DistortionStrength`) UV parallax → `MainTexture` | `[VERIFIED: MaterialExpressionBumpOffset.h:13-35]` — not true refraction |
| `flipbook_subuv` | `MainTexture` via `TextureSampleParameterSubUV` | `[VERIFIED: MaterialExpressionTextureSampleParameterSubUV.h:14-34]` |

Masters are named `{M_Ueremcp_ProjCore|ProjTrail}_{FeatureSignature}` so graph variants do not collide.
Purpose defaults live in `element_presets.v1.json` → `purpose_default_features` (loaded at runtime with C++ fallback).

Not yet wired: engine MaterialFunctions.

`textures.generate` slots (`noise`, `gradient`, `voronoi`, `ring_mask`, `flow_map`) execute via `create_procedural_texture` and bind to MI texture parameters (`MainTexture`, `NoiseTexture`, `FlowMap`, `MaskTexture`).

## Epic tool composition (implementation note)

UEREMCP does **not** re-expose MaterialTools' 29 primitives. Internal batching via `ProgrammaticToolset.execute_tool_script`:

1. `MaterialTools.create_material` / `create_function` (masters only under test paths)
2. `add_expression`, `connect_expressions`, `connect_to_output`
3. `MaterialInstanceTools.create`, parameter setters
4. `recompile` — must succeed before `created_and_validated`
5. Hide primitives with `SetNameFilters` (ADR-0002)

## Fidelity (honest defaults until round-trip proven)

| Lossy area key | Meaning |
|---|---|
| `expression_subclass_properties` | MaterialTools has no expression property setter |
| `material_function_internals` | Nested MF graphs need separate retrieve |
| `editor_chrome` | Comments, preview settings, layout beyond x/y |

These keys match `UeremcpMaterialCapabilityNotes.h` and `create_vfx_material` `capability_notes`.

## Known gaps (capability_notes)

| Gap | Severity | Mitigation |
|---|---|---|
| MaterialTools omits blend/shading/domain | Mitigated | Set on `UMaterial` before recompile in feature graph builder |
| Feature tokens not wired | Mitigated (projectile slice) | See feature table above; unimplemented tokens → `created_with_warnings` |
| Procedural texture generation | Mitigated | `create_procedural_texture` + `textures.generate` slots (CPU FImageUtils path) |
| Element presets at runtime | Mitigated | `element_presets.v1.json` via `UeremcpMaterialElementPresetsLoader` with C++ fallback |
| Substrate shading overrides | Medium | Per-material runtime check in RE project |
| Graph round-trip unproven | Medium | WS-11 harness under `/Game/__UeremcpTests/` |

Full research: `docs/research/RB-08-materials-and-textures.md`.

## Tests

Runtime probes and created assets: **`/Game/__UeremcpTests/Materials/**` only.

```bash
python tools/validate_schemas.py
python schemas/domains/materials/test_specifications.py
python schemas/domains/materials/test_element_presets.py
python schemas/domains/materials/test_element_presets_loader.py
python schemas/domains/materials/test_features.py
python schemas/domains/materials/test_procedural_texture.py
python schemas/domains/materials/test_niagara_export.py
python schemas/domains/materials/test_validate_contract.py
python tools/check_ownership.py --ws WS-08
```

Editor automation (RE project + compiled plugin):

```bash
# WS-11 harness — UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore|ProjectileTrail
```
