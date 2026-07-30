# RB-08: Material graph authoring, procedural textures, VFX asset generation

- **Owner:** WS-08
- **Status:** not_started
- **Blocks:** POC B (Niagara needs materials), master prompt §6
- **Priority:** high

## Framing

Niagara effects are mostly materials. A fireball that compiles but renders as an
untextured sprite is not a fireball. POC B therefore depends on this brief as much as
on RB-07.

REAgentTools has material *instance* support but explicitly no master-graph editing
`[VERIFIED: $RAT/Docs/CAPABILITY_MATRIX.md]`. Epic reportedly has `MaterialTools`
`[UNVERIFIED — pending RB-02]`.

## Questions

### A. Material graphs

1. What public/editor API creates a `UMaterial` and adds `UMaterialExpression` nodes,
   connects them, and compiles? Is it materially easier or harder than Blueprint graph
   authoring (RB-05)?
2. Can material graphs be read into `graph.schema.json` with `graph_type: MaterialGraph`,
   and does the `nodes`/`links` shape fit material expressions well? Report honestly.
3. How are material *attributes* / the result node connected, and how are blend mode,
   shading model, translucency, and two-sidedness set?
4. Material functions — can they be created and instanced? Composing functions is likely
   far more reliable than authoring large expression graphs, and would be a good finding.
5. Material instances and dynamic parameters — how are scalar/vector/texture parameters
   exposed and named so Niagara can drive them?
6. How is compilation/validation confirmed, and how do shader compile errors surface?

### B. The VFX material feature set

Design `create_vfx_material` around the master prompt §6 feature list. For each, find
the expression graph pattern that implements it and record it as a reusable snippet:

radial falloff · animated noise · fresnel · erosion / dissolve · depth fade ·
distortion · panning textures · flow maps · flipbook / SubUV · signed distance field
shapes · procedural shapes · radial gradients · voronoi · normal distortion ·
colour ramps · dynamic colour · dynamic intensity · additive vs translucent vs masked ·
ribbon-specific · mesh-particle-specific · decal · beam profile

7. Which of these are available as engine material functions already (so we compose
   rather than author)? **Check this before writing any expression-graph builder** — it
   is likely the difference between a week and a month of work.
8. What is the minimum set of *master* materials from which most VFX needs are met via
   instances? A small master set plus instances is far more maintainable than generating
   a bespoke graph per effect, and much more likely to be correct.

### C. Texture and mesh generation

9. Can textures be generated procedurally in-engine — noise, gradients, voronoi, rune
   shapes, ring/band masks? Investigate `UTexture2D` from raw data, render targets +
   `DrawMaterialToRenderTarget`, and whether the result can be saved as a persistent
   asset.
10. Flipbook / SubUV atlas assembly — possible in-engine, or does it need an external
    tool?
11. Helper meshes (rings, bands, beam tubes, magic-circle discs) — generatable via
    procedural mesh / Modeling Tools (`ModelingToolsEditorMode` is enabled
    `[VERIFIED: RE.uproject]`), or better sourced externally?
12. When generation must be external, what is the structured asset-request format, and
    what is the import-and-configure path? Coordinate with RB-11 (Blender/MCP client).
13. `$PROJ/ComfyUI` exists — is there an existing image-generation pipeline we should
    integrate with rather than duplicate? **Check before designing anything new.**

## Deliverables

- [ ] Verdict on ADR-0004 fit for material graphs
- [ ] A master-material set proposal, with the instance parameters each exposes
- [ ] `create_vfx_material` specification schema in `schemas/domains/materials/`
- [ ] A feature → implementation-pattern table for the §6 list
- [ ] Working procedural texture generation, or a documented negative finding
- [ ] Materials POC B needs, delivered to WS-07
