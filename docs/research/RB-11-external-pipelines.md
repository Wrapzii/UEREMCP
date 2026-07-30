# RB-11: External asset pipelines and `MCPClientToolset`

- **Owner:** WS-08
- **Status:** not_started
- **Blocks:** master prompt §6 external generation path
- **Priority:** medium

## Framing

When a required texture or mesh does not exist, master prompt §6 wants the system to
generate it procedurally, or produce a structured asset-generation request and use an
external pipeline (Blender or similar), import, configure, and record the dependency.

Two existing facts make this cheaper than building from scratch:

- `MCPClientToolset` ships in the engine and is **enabled** in RE
  `[VERIFIED: RE.uproject]`. If it lets the editor act as an MCP *client*, Unreal could
  call out to other MCP servers directly.
- `$PROJ/.mcp.json` already configures a `blender` MCP server via `uvx blender-mcp`
  `[VERIFIED]`. And `$PROJ/ComfyUI` exists, suggesting an image-generation pipeline is
  already in play.

**Check what already works before designing anything.**

## Questions

1. What does `MCPClientToolset` actually do? Can the editor call an external MCP server
   from inside a tool call? If yes, "Unreal asks Blender for a mesh" becomes one
   operation rather than a round trip back through the agent — which matters a great deal
   under the cost model in `docs/WHY.md`.
2. Is the `blender-mcp` server functional in this environment, and what can it do?
3. What is in `$PROJ/ComfyUI` — a working texture-generation pipeline? Who drives it
   today? **Do not build a second image pipeline if one works.**
4. What is the import path for externally generated assets — Interchange, `AssetTools`
   import, FBX/glTF/PNG — and how much configuration can be automated (compression,
   sRGB, LOD, collision, material assignment)?
5. Can import be done fully headlessly without modal dialogs? Import dialogs are a known
   hang source (RB-04 q17).
6. What is the structured asset-generation request format? It should be expressible as a
   batch operation so a Niagara build can declare "I need a rune texture" and have it
   satisfied inside the same plan.
7. How are externally generated assets recorded as dependencies, and how is provenance
   tracked so a regenerated texture does not silently change an effect?
8. Failure handling: what happens when the external tool is unavailable? The operation
   must degrade to a clear `unresolved_dependencies` entry, never a silent placeholder.
   A silently-substituted placeholder that compiles is exactly the failure mode this
   project exists to eliminate.

## Deliverables

- [ ] A verdict on `MCPClientToolset` and whether editor→external-MCP calls work
      — flag to WS-01 if they do; it affects the batching design
- [ ] An inventory of pipelines that already exist and work
- [ ] The structured asset-request format, as a batch operation
- [ ] A working import-and-configure path, headless
- [ ] Provenance/dependency recording design
