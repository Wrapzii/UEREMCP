# RB-08: Material graph authoring, procedural textures, VFX asset generation

- **Owner:** WS-08
- **Status:** research_complete_pending_gates
- **Blocks:** POC B (Niagara needs materials), master prompt §6
- **Priority:** high
- **Last updated:** 2026-07-29 (WS-08 research run)

## Executive summary

Material graph authoring in UE 5.8 is **materially easier than Blueprint graph authoring** for round-trip: the graph is data-only (no execution pins, no latent nodes, no delegates), and Epic already ships a **complete primitive surface** via `MaterialTools` + `MaterialEditingLibrary` that covers create, read, connect, disconnect, compile-with-errors, material functions, and material instances. REAgentTools correctly stops at material-instance workflows.

**UEREMCP should not duplicate Epic's `MaterialTools` primitives.** The WS-08 deliverable is semantic goal-level tools (`create_vfx_material`, `instantiate_element_material`, procedural texture helpers) that batch Epic primitives internally, return complete graph JSON where needed, and verify compile success before reporting `created_and_validated`.

ADR-0004's shared `nodes`/`links` representation **fits material expression graphs** with two extensions: (1) material-output-property links, and (2) per-expression `properties` for subclass-specific fields. Round-trip is **expected achievable** for expression graphs; lossy areas are editor chrome, not shader logic.

The maintainable VFX strategy is a **small master-material set + instances + element-parameter templates** (fire/water/wind/earth vary data, not tooling). Procedural textures are **feasible in-editor** via `ImageUtils` / render-target draw + save; flipbook atlas assembly is **partial** (no dedicated Epic tool). Helper meshes are **out of scope for MaterialTools** — ComfyUI/Hunyuan3D exists in `$PROJ` for hero 3D; simple rings/discs need Modeling Tools or Blender (RB-11).

**Runtime verification this run:** UE MCP connection failed (`Connection closed`). All API claims below are header/source reads unless tagged `[VERIFIED-RUNTIME]`. Editor integration tests are gated on WS-11 harness; assets must live under `/Game/__UeremcpTests/` only.

---

## Framing

Niagara effects are mostly materials. A fireball that compiles but renders as an untextured sprite is not a fireball. POC B depends on this brief as much as RB-07.

REAgentTools has material *instance* support but explicitly no master-graph editing `[VERIFIED: $RAT/Docs/CAPABILITY_MATRIX.md]`. Epic ships `MaterialTools` in `EditorToolset` `[VERIFIED: $ENGINE/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/material.py]`.

---

## A. Material graphs — answers

### A1. Public API for create / add expressions / connect / compile

**Verdict: fully covered by Epic; easier than Blueprint (RB-05).**

| Layer | Role | Verification |
|---|---|---|
| `MaterialTools` (Python toolset) | Agent-facing primitives: asset create, expression CRUD, wiring read/write, recompile | `[VERIFIED: material.py]` |
| `UMaterialEditingLibrary` (C++) | Substrate all graph mutations | `[VERIFIED: MaterialEditingLibrary.h]` |
| `MaterialInstanceTools` | MI create, list/get/set parameters, static switches | `[VERIFIED: material_instance.py]` |

**Create:** `MaterialTools.create_material`, `create_function`, `create_parameter_collection` `[VERIFIED: material.py:31-84]`.

**Add expressions:** `add_expression(material_or_function, expression_class, x, y)` dispatches to `CreateMaterialExpression` / `CreateMaterialExpressionInFunction` `[VERIFIED: material.py:123-154]`, `[VERIFIED: MaterialEditingLibrary.h:167-173]`, `[VERIFIED: MaterialEditingLibrary.h:372-373]`.

**Connect:** `connect_expressions`, `connect_to_output` (material property inputs), `disconnect_*` `[VERIFIED: material.py:310-466]`, `[VERIFIED: MaterialEditingLibrary.h:231-260]`.

**Compile / validate:** `MaterialTools.recompile` calls `RecompileMaterial`; **returns error strings; raises `RuntimeError` on failure** `[VERIFIED: material.py:486-513]`, `[VERIFIED: MaterialEditingLibrary.h:266-267]`. Epic's own tests confirm invalid HLSL fails recompile `[VERIFIED: test_material.py:160-167]`.

**Discover expressions:** `list_expression_classes(material_or_function, search)` filters `MaterialExpression` subclasses; excludes `FunctionInput`/`FunctionOutput` on materials `[VERIFIED: material.py:92-119]`.

**Compared to Blueprint (RB-05):** Material graphs have no execution flow, no latent nodes, no custom K2 nodes, no delegate pins. The dominant complexity is **expression subclass property diversity** (each `MaterialExpression*` has different `set_editor_property` fields), not graph topology. MaterialTools does **not** wrap property assignment — callers use `expression.set_editor_property(...)` as Epic tests do `[VERIFIED: test_material.py:21-27]`.

### A2. ADR-0004 fit — `graph_type: MaterialGraph`

**Verdict: fits with extensions; honest fidelity declaration required.**

| `graph.schema.json` field | Material mapping |
|---|---|
| `nodes[]` | Each `UMaterialExpression` → one node; `node_class` = expression class path; `properties` = expression UObject fields |
| `links[]` | Expression-to-expression wires + expression-to-output-property wires |
| `input_pins` / `output_pins` | From `GetMaterialExpressionInputNames` / `GetMaterialExpressionOutputNames` `[VERIFIED: MaterialEditingLibrary.h:324-333]` |
| `entry_points` / `exit_points` | Material: `MP_*` output properties with connections; Function: `FunctionOutput` nodes |
| `execution_paths` | Empty (data-only graph) |
| `data_paths` | Derivable from links toward material outputs |
| `diagnostics` | Populate from `RecompileMaterial` errors + disconnected outputs + unused expressions |

**Extensions (propose via WS-01 when implementing, not in this research run):**

```json
"extensions": {
  "material": {
    "blend_mode": "BLEND_Additive",
    "shading_model": "MSM_Unlit",
    "two_sided": true,
    "material_domain": "MD_Surface",
    "output_property_links": [
      { "property": "MP_EmissiveColor", "from_node_semantic_id": "...", "from_output": "RGB" }
    ]
  }
}
```

**Lossy areas (declare in `fidelity.lossy_areas`):**

- Expression preview / thumbnail settings
- Comment boxes (if not mapped to `comments[]`)
- Editor-only graph layout beyond x/y
- Substrate-specific expression variants (UE 5.8 Substrate enabled in project — verify per-material at runtime)
- Nested material-function **internal** graphs when referenced via `MaterialFunctionCall` (fetch as separate `MaterialFunctionGraph` subgraph)

**Round-trip test expectation:** retrieve → `mode: replace` → retrieve → same `content_hash`. **Anticipated pass** for pure expression graphs built only from public API. **Not claimed** until WS-11 harness runs in `/Game/__UeremcpTests/`.

### A3. Material attributes / result node / blend mode / shading / translucency / two-sided

**Output properties:** Connected via `connect_to_output(expression, output_name, material_property)` where `material_property` is `EMaterialProperty` (e.g. `MP_EmissiveColor`, `MP_Opacity`, `MP_BaseColor`) `[VERIFIED: material.py:431-447]`, `[VERIFIED: MaterialEditingLibrary.h:231-232]`.

**Read output wiring:** `get_property_input(material, material_property)` → `MaterialInputSource` `[VERIFIED: material.py:399-423]`.

**Blend mode, shading model, two-sided, translucency:** Stored on `UMaterial` UObject properties `[VERIFIED: Material.h:482-561]` — `BlendMode`, `ShadingModel`/`ShadingModels`, `TwoSided`, `TranslucencyPass`, `TranslucencyLightingMode`, etc.

**Gap:** `MaterialTools` has **no** tools for these. Implementation must call `material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)` (or equivalent) then `recompile`. Same for `MaterialUsage` flags (sprite/mesh particle) via `SetBaseMaterialUsage` `[VERIFIED: MaterialEditingLibrary.h:204-205]`.

**Material instances** can override blend mode, shading model, two-sided when parent allows `[VERIFIED: MaterialInstance editor customization references in MaterialEditorInstanceDetailCustomization.cpp — blend/shading/two-sided override rows]`.

### A4. Material functions — create, instance, compose

**Verdict: supported; preferred composition strategy.**

| Operation | API | Verification |
|---|---|---|
| Create empty function | `MaterialTools.create_function` | `[VERIFIED: material.py:50-62]` |
| Add FunctionInput/Output | `add_expression` with `MaterialExpressionFunctionInput`/`Output` | `[VERIFIED: test_material.py:453-467]` |
| Wire inside function | Same connect/read APIs on function context | `[VERIFIED: test_material.py:560-570]` |
| Update + recompile dependents | `recompile` on function → `UpdateMaterialFunction` | `[VERIFIED: material.py:505-508]` |
| Reference in material | `MaterialExpressionMaterialFunctionCall` + `set_editor_property('material_function', func)` | `[VERIFIED: test_material.py:572-585]` |
| Find referencers | `get_referencing_materials(material_function)` | `[VERIFIED: material.py:517-528]` |

Epic's `MaterialBasicsSkill` explicitly instructs agents to **search existing MaterialFunctions before hand-authoring** `[VERIFIED: material_basics.py:14-16]`.

**Engine content:** Standard library lives under `/Engine/Functions/` (content browser path). Installed-build enumeration of `.uasset` names is unreliable from filesystem alone; **runtime AssetRegistry scan** under `/Engine/Functions` is the verification path — gated on editor harness.

### A5. Material instances and dynamic parameters for Niagara

**Epic:** `MaterialInstanceTools` — `create`, `list_parameters`, get/set scalar/vector/texture/static_switch, `set_parent`, `clear_parameters` `[VERIFIED: material_instance.py]`.

**REAgentTools:** `REMaterialWorkflowTools` — `create_material_instance_configure_save`, `update_material_instance_parameters`, `create_assign_material_instance` using `MaterialEditingLibrary` directly `[VERIFIED: material_workflow_tools.py]`.

**Parameter naming:** Defined on master material `MaterialExpressionScalarParameter` / `VectorParameter` / `TextureSampleParameter` nodes (`parameter_name` property). Niagara renderer binds to these names on the assigned `MaterialInterface`. Convention in RE capture workflow: `ParticleColor` as vector override `[VERIFIED: $RAT/Docs/VISUAL_LOOP.md via capture_workflow_tools.py ParticleColor defaults]`.

**Dynamic material instances:** `create_dynamic_material_instance` on mesh components for runtime preview `[VERIFIED: capture_workflow_tools.py:710]`. Production Niagara uses MI or MID on renderer material slot.

**Static switches:** Changing them triggers full shader recompile; `MaterialInstanceTools.set_static_switch_parameter` explicitly recompiles base material `[VERIFIED: material_instance.py:357-358]`. Minimize static switches on shared masters (Epic skill + material_basics.py:23-24).

### A6. Compilation validation and error surfacing

| Check | Mechanism | Verification |
|---|---|---|
| Shader compile success | `RecompileMaterial` → empty error array | `[VERIFIED: MaterialEditingLibrary.h:266-267]` |
| Shader compile failure | `MaterialTools.recompile` raises with joined errors | `[VERIFIED: material.py:499-502]` |
| Statistics | `GetStatistics(material)` — instruction counts, samplers | `[VERIFIED: MaterialEditingLibrary.h:609-610]` |
| Shader listing | `ListShaders(material)` | `[VERIFIED: MaterialEditingLibrary.h:605-606]` |
| Visual validation | REAgentTools `render_material_preview_to_disk` — lit plane + JPEG | `[VERIFIED: $RAT capture_workflow_tools.py]` |

**UEREMCP validation bar:** `created_and_validated` requires recompile success **plus** re-read of output property wiring and parameter exposure **plus** save. Optional preview render when WS-11 harness supports it.

---

## B. VFX material feature set (master prompt §6)

### B7. Engine material functions — compose vs author

**Rule:** Audit `/Engine/Functions` and `/Engine/Functions/MaterialLayerFunctions` at runtime before writing expression builders.

| Feature (§6) | Typical engine MF / pattern | Compose? | Notes |
|---|---|---|---|
| Radial falloff / gradient | `/Engine/Functions/.../RadialGradient`, `SphereMask` expressions | Yes | Search AssetRegistry |
| Animated noise | `MaterialExpressionNoise` or MF noise | Yes | Time via `MaterialExpressionTime` |
| Fresnel | Fresnel MF family | Yes | Common in `/Engine/Functions` |
| Erosion / dissolve | Custom or MF edge dissolve | Partial | Often project-specific |
| Depth fade | DepthFade MF / `PixelDepth` | Yes | Particle cam offset |
| Distortion | Refraction pin + normal perturbation | Partial | Needs translucent + refraction enabled |
| Panning textures | `Panner` MF or `TextureCoordinate` + `Speed` params | Yes | |
| Flow maps | FlowMap MF / vector offset | Yes | |
| Flipbook / SubUV | `SubUV` function or `ParticleSubUV` expr | Yes | Needs atlas texture |
| SDF shapes | `SphereMask`, `BoxMask` expressions | Yes | |
| Procedural shapes | Shape MFs | Yes | |
| Voronoi | Voronoi MF / `MaterialExpressionVoronoiNoise` | Yes | |
| Normal distortion | PerturbNormal MF | Yes | |
| Colour ramps | `CurveAtlas` or `Gradient` expr | Yes | |
| Dynamic colour / intensity | Vector/scalar parameters | Yes | Instance layer |
| Additive / translucent / masked | `BlendMode` on material | Yes | Master template switch |
| Ribbon-specific | UV from ribbon tessellation + stretch MF | Partial | Master `M_VFX_Ribbon` |
| Mesh-particle-specific | Vertex color / normals | Partial | Usage flags |
| Decal | `MaterialDomain` = DeferredDecal | Yes | Separate master |
| Beam profile | Cylinder/box mask along U | Partial | Often custom MF |

**Installed-build filesystem scan of `.uasset` names failed silently this run.** Treat the table as **pattern guidance**; WS-08 implementation must run AssetRegistry query in editor and fill `docs/audit/` row via proposal.

### B8. Minimum master-material set

**Proposal: 6 masters + instances (not 6 bespoke graphs per effect).**

| Master | Domain / blend | Primary outputs | Key instance parameters |
|---|---|---|---|
| `M_Ueremcp_VFX_Sprite_Additive` | Surface, Unlit, Additive, sprite usage | EmissiveColor, Opacity | ParticleColor, EmissiveScale, SoftEdge, DepthFade, DistortionStr, MainTex, NoiseTex |
| `M_Ueremcp_VFX_Sprite_Translucent` | Surface, Unlit, Translucent | EmissiveColor, Opacity | Same + Refraction (if distortion) |
| `M_Ueremcp_VFX_Sprite_Masked` | Surface, Unlit, Masked | EmissiveColor, OpacityMask | EdgeSharpness, MainTex |
| `M_Ueremcp_VFX_Mesh_Unlit` | Surface, Unlit, mesh particle usage | EmissiveColor, Opacity | ParticleColor, NormalTex, EmissiveScale |
| `M_Ueremcp_VFX_Ribbon` | Surface, Unlit, Additive/Translucent | EmissiveColor, Opacity | RibbonWidth, FadeEnds, ParticleColor |
| `M_Ueremcp_VFX_Decal` | DeferredDecal | EmissiveColor, Opacity | Color, MaskTex, Dissolve |

Optional seventh: `M_Ueremcp_VFX_Distortion` (translucent refraction-only) for heat haze.

**Construction:** Each master is assembled from **engine MaterialFunctions** where possible; shared subgraph stored as `/Game/__UeremcpTests/Materials/Functions/MF_Ueremcp_VFX_Core` (test path only until promoted).

---

## C. Texture and mesh generation

### C9. Procedural textures in-engine

**Verdict: feasible with documented pipeline; not a single Epic tool.**

| Approach | API | Persist? | Verification |
|---|---|---|---|
| Raw pixel buffer → `UTexture2D` | `UImageUtils::CreateTexture2D` / `CreateTexture` | Yes after `PostEditChange` + `EditorAssetSubsystem.save_asset` | `[VERIFIED: ImageUtils.h:268-291]` — **WITH_EDITOR only** |
| Transient from image | `CreateTexture2DFromImage` — **Transient** | Must copy/save explicitly | `[VERIFIED: ImageUtils.h:292]` |
| Draw material → RT | `UKismetRenderingLibrary::DrawMaterialToRenderTarget` | RT is transient; export via `ExportTexture2D` or factory save | `[VERIFIED: KismetRenderingLibrary.h:81-82]`, `[VERIFIED: KismetRenderingLibrary.h:203-204]` |
| Import image bytes | `ImportBufferAsTexture2D` | Save asset | `[VERIFIED: KismetRenderingLibrary.h:216]` |

**Negative:** No Epic toolset tool for "generate noise texture" or "save RT as asset" — UEREMCP must implement semantic `create_procedural_texture` wrapping the above.

**Noise / gradient / voronoi / ring masks:** Generate pixel data in C++ or Python (`numpy` if available in editor Python), call `CreateTexture2D`, set `CompressionSettings` (e.g. `TC_Grayscale`, `TC_VectorDisplacementmap` for flow), save under `/Game/__UeremcpTests/Textures/`.

### C10. Flipbook / SubUV atlas assembly

**Verdict: partial in-engine; no dedicated Epic tool.**

- **In-engine:** Slice grid into subregions via image processing → single atlas `UTexture2D`; material uses SubUV MF or `MaterialExpressionParticleSubUV`.
- **External:** ComfyUI / image editor for artistic flipbooks; import via `Texture2DFactoryNew` / `ImportBufferAsTexture2D`.
- **Negative:** No `MaterialTools` or `AssetTools` flipbook assembler found in EditorToolset.

### C11. Helper meshes (rings, bands, beam tubes, discs)

**Verdict: not MaterialTools scope; multiple paths.**

| Path | Fit | Verification |
|---|---|---|
| Modeling Tools Editor Mode | Enabled in `$PROJ` `[VERIFIED: GROUNDED_FACTS.md §5]` | Interactive; poor for deterministic agent batch |
| ProceduralMeshComponent | Runtime mesh sections | `[VERIFIED: MRMeshComponent.h references ProceduralMeshComponent pattern]` |
| ComfyUI Hunyuan3D | Hero props, not FX rings | `[VERIFIED: $PROJ/Tools/ComfyWorkflows, Content/Python/comfy]` |
| Blender MCP (RB-11) | Structured mesh requests | Coordinate via proposal |

**Recommendation:** Simple parametric meshes (ring, disc, tube) as **small static mesh library** under `/Game/__UeremcpTests/Meshes/` built once; agent varies scale in Niagara not geometry.

### C12. External asset request format

Coordinate with RB-11. Minimum structured request:

```json
{
  "asset_kind": "texture|static_mesh|flipbook_atlas",
  "semantic_role": "flow_map|noise|mask|helper_mesh_ring",
  "dimensions": [512, 512],
  "format": "png",
  "import_path": "/Game/__UeremcpTests/Imported/",
  "material_binding": { "parameter": "FlowMap" }
}
```

### C13. `$PROJ/ComfyUI` pipeline

**Verdict: integrate for hero content; do not duplicate for procedural masks.**

- ComfyUI API at `http://127.0.0.1:8001` documented in `$PROJ/Content/RE/AGENT_STARTUP_PROMPT.md`.
- Hunyuan3D workflows in `$PROJ/Tools/ComfyWorkflows/` for textured GLB meshes.
- **Not** the right default for 512² noise/gradient masks (latency, queue contention). Use in-engine generation for deterministic test assets.

---

## D. Element-parameter model (fire / water / wind / earth)

**Design intent:** One tooling surface; templates vary **semantic element bindings** and default parameters, not duplicate `create_vfx_material` per element.

### D1. Layers

| Layer | Owner | Content |
|---|---|---|
| **Master materials** | WS-08 | Blend/domain/usage; shared MF wiring |
| **Element templates** | WS-15 + WS-08 | `element_kind` → default parameter map + texture slots |
| **Effect instances** | Agent request | `create_vfx_material` with `element: fire` + overrides |
| **Niagara binding** | WS-07 | Renderer material = MI path; drives same parameter names |

### D2. Shared semantic parameters (cross-element)

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
| `MainTexture` | Texture | Primary sample |
| `NoiseTexture` | Texture | Secondary noise |
| `FlowMap` | Texture | Vector flow |
| `MaskTexture` | Texture | Shape/ring/band |
| `UseDistortion` | StaticSwitch | Permutation control — use sparingly |

### D3. Element default profiles (template data, not code)

| `element_kind` | ColorPrimary | ColorSecondary | FlowSpeed | Turbulence | Notes |
|---|---|---|---|---|---|
| `fire` | (1.0, 0.35, 0.05) | (1.0, 0.8, 0.2) | 0.3 upward bias | 0.8 | High EmissiveScale; additive |
| `water` | (0.1, 0.4, 0.8) | (0.8, 0.9, 1.0) | 0.5 lateral | 0.4 | Translucent; refraction optional |
| `wind` | (0.7, 0.85, 1.0) | (0.9, 0.95, 1.0) | 1.2 | 0.9 | Low opacity; motion-led |
| `earth` | (0.3, 0.25, 0.2) | (0.5, 0.45, 0.35) | 0.1 | 0.3 | Masked or opaque debris; low emissive |

Stored as JSON in `templates/elements/` (WS-15) referencing master material path + parameter defaults. **No frozen schema change in this research run.**

### D4. Semantic tool sketch (implementation post-gates)

```
create_vfx_material:
  specification:
    element_kind: fire | water | wind | earth | custom
    master_template: optional override (default from element)
    instance_name, folder_path (under allowed roots)
    parameter_overrides: { ... }
    textures: { MainTexture: path | generate_procedural: noise }
    validate: { recompile: true, preview: optional }
```

Returns envelope with MI path, parameter manifest, compile diagnostics, `content_hash` of master graph if modified.

---

## E. Audit — Epic vs REAgentTools vs UEREMCP

### Epic `MaterialTools` inventory (verified tool names)

From `material.py` — **29 tool_call methods:**

`create_material`, `create_function`, `create_parameter_collection`, `list_expression_classes`, `add_expression`, `delete_expression`, `get_expressions`, `layout_expressions`, `list_parameter_groups`, `rename_parameter_group`, `delete_parameter_group`, `get_expression_input_names`, `get_expression_output_names`, `connect_expressions`, `disconnect_expressions`, `get_expression_inputs`, `get_property_input`, `connect_to_output`, `disconnect_from_output`, `delete_unused_expressions`, `recompile`, `get_referencing_materials`.

Plus `MaterialInstanceTools`: `create`, `list_parameters`, get/set scalar/vector/texture/static_switch, `set_parent`, `clear_parameters`, `set_parameter_override`.

**Disposition for UEREMCP:** **Preserve** — hide via `SetNameFilters`; call internally from semantic tools. **Do not** register as agent-facing MCP tools.

### REAgentTools `REMaterialWorkflowTools`

| Tool | Disposition |
|---|---|
| `create_material_instance_configure_save` | **Improve** — wrap in envelope; keep workflow |
| `update_material_instance_parameters` | **Improve** |
| `assign_materials_to_mesh_components` | **Preserve** in RE or gameplay WS |
| `create_assign_material_instance` | **Improve** — composite stays valid |
| Master graph edit | **Absent** — correctly defers to Epic |

**Reuse:** `_set_mi_params` JSON pattern, `workflow_result` shape, preview capture integration.

### What UEREMCP adds (post-gates)

| Capability | Why Epic/RE insufficient alone |
|---|---|
| `create_vfx_material` semantic op | Batches 20–80 MaterialTools calls → 1 envelope |
| Complete graph JSON round-trip | ADR-0004; Epic returns UObject refs not schema |
| Element template instantiation | WS-15 integration |
| Procedural texture semantic op | No Epic tool |
| Verified compile + diagnostics in response | Epic raises; we need honest statuses |
| Idempotency / revision / rollback | ADR-0005/0006 |

Full audit rows submitted via `docs/proposals/ws-08-epic-material-audit.md` (WS-02 to merge).

---

## F. Safe deterministic creation

| Rule | Mechanism |
|---|---|
| Test assets only | `/Game/__UeremcpTests/Materials/**`, `/Textures/**`, `/Meshes/**` |
| No user asset deletion | `dry_run` default for destructive graph replace |
| Stable paths | `folder_path` + `asset_name` + idempotency key (ADR-0006) |
| Delete-and-recreate graph | Acceptable for determinism inside owned test assets |
| Transactions | `ScopedTransaction` / editor transactions (ADR-0005) |
| Verify before success | `recompile` + re-read wiring + save |
| Static switch budget | ≤2 per master to limit permutation explosion |

---

## G. Negative findings

1. **No runtime MCP verification** this run — UE editor MCP `Connection closed`.
2. **MaterialTools does not set** blend mode, shading model, two-sided, translucency, material domain — `set_editor_property` required.
3. **No expression property setter** in MaterialTools — all subclass fields manual.
4. **No procedural texture tool** in Epic toolsets.
5. **No flipbook assembler** in Epic toolsets.
6. **Helper mesh generation** not in MaterialTools; Modeling Tools not batch-friendly.
7. **Engine MF catalog** not enumerated from installed build filesystem; needs AssetRegistry in editor.
8. **Substrate** may alter shading model override rules — project-specific runtime check required.
9. **`CreateTexture2D` / `CreateTexture`** editor-only; not for packaged game procedural gen without different pipeline.
10. **ComfyUI** wrong tool for small deterministic masks (queue latency).

---

## H. Implementation plan (conditional on Phase 1 gates)

### Gates required

| Gate | From | Blocks |
|---|---|---|
| `UeremcpCore` toolset + envelope | WS-03, WS-05 | Any implementation |
| Editor test harness | WS-11 | Verified round-trip tests |
| Audit matrix merge | WS-02 | Disposition sign-off |
| POC A learnings | WS-06 | Graph serializer patterns |

### Phase 2 WS-08 sequence

1. **Graph read path** — `UeremcpMaterial`: Material → `graph.schema.json` (`MaterialGraph` / `MaterialFunctionGraph`).
2. **Graph write path** — `mode: replace` via MaterialEditingLibrary; delete-all-expressions + rebuild strategy for determinism.
3. **Master library** — build 6 test masters under `/Game/__UeremcpTests/Materials/Masters/` using internal Epic tool calls.
4. **Element templates** — proposal to WS-15 for `templates/elements/*.json`.
5. **`create_vfx_material`** — semantic tool; returns MI + diagnostics; uses element-parameter model.
6. **`create_procedural_texture`** — ImageUtils / RT path; save + validate.
7. **POC B handoff to WS-07** — MI paths for default sprite/mesh/ribbon materials.
8. **Tests** — round-trip, compile failure reporting, element fire/water instance defaults.

### Proposals (not implementation)

- `schemas/domains/materials/create_vfx_material.schema.json` — after WS-05 gate.
- `schemas/graph/extensions/material.schema.json` — WS-01 approval.
- WS-15 element template store entries.

---

## Deliverables checklist

- [x] Verdict on ADR-0004 fit for material graphs (§A2)
- [x] Master-material set proposal (§B8)
- [ ] `create_vfx_material` specification schema — **gated** (WS-05)
- [x] Feature → implementation-pattern table (§B7)
- [x] Procedural texture generation verdict (§C9 — feasible with pipeline)
- [ ] Materials POC B needs — **handoff doc after masters exist** (WS-07 dependency)

---

## Open questions

1. AssetRegistry MF catalog under `/Engine/Functions` — needs editor run.
2. Substrate interaction with Unlit/Additive VFX masters in RE project.
3. Whether WS-06 graph serializer is directly reusable vs material-specific adapter.
4. Niagara default material paths in RE project for POC B baseline.
