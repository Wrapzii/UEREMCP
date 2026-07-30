# Research Briefs

One brief per workstream. **You own your own brief file.** Follow
`../RESEARCH_PROTOCOL.md` — every API claim carries a verification tag.

Do not skip your brief and go straight to implementation. The briefs exist because
this project's failure mode is building on an API that turns out not to exist.

## Index

| Brief | Owner | Topic | Blocks | Priority |
|---|---|---|---|---|
| [RB-01](RB-01-engine-baseline.md) | WS-01 | Engine version, source availability, API stability | ADR-0001 confidence | high |
| [RB-02](RB-02-epic-toolset-inventory.md) | WS-02 | Actual tool inventory of Epic's 27 toolsets | **every domain workstream** | **highest** |
| [RB-03](RB-03-plugin-integration.md) | WS-03 | `UToolsetDefinition` / `AICallable` mechanics from an out-of-tree plugin | ADR-0002, ADR-0007, all tools | **highest** |
| [RB-04](RB-04-transport-and-jobs.md) | WS-04 | Transport options, progress, cancellation, long-running jobs | ADR-0009 | high |
| [RB-05](RB-05-blueprint-graph-roundtrip.md) | WS-06 | Blueprint graph serialise / reconstruct fidelity | **POC A, the core thesis** | **highest** |
| [RB-06](RB-06-sandbox-and-rollback.md) | WS-11 | `FileSandbox` semantics vs package saves, registry, compilation | ADR-0005, all batching | **highest** |
| [RB-07](RB-07-niagara.md) | WS-07 | Niagara system/emitter/module-stack read and write | POC B, POC C | high |
| [RB-08](RB-08-materials-and-textures.md) | WS-08 | Material graph authoring, procedural textures, flipbooks | POC B dependencies | high |
| [RB-09](RB-09-animation-controlrig.md) | WS-10 | AnimBP, state machines, Control Rig, IK, retargeting | Wave 3 design | medium |
| [RB-10](RB-10-template-substrate.md) | WS-15 | `UAgentSkill` as template substrate; library design | ADR-0008 | high |
| [RB-11](RB-11-external-pipelines.md) | WS-08 | `MCPClientToolset`, Blender bridge, asset import | §6 asset generation | medium |
| [RB-12](RB-12-gas-and-gameplay.md) | WS-09 | GAS authoring, `GASToolsets`, replication validation | POC D | medium |
| [RB-13](RB-13-security.md) | WS-12 | Auth, allowed roots, arbitrary execution, audit | ADR-0010 | high |
| [RB-14](RB-14-testing-automation.md) | WS-11 | Automation framework, editor tests, PIE smoke tests | all verification | high |
| [RB-15](RB-15-reagenttools-migration.md) | WS-02 | REAgentTools disposition and migration | migration plan | high |

## Sequencing

**Start immediately, in parallel:** RB-02, RB-03, RB-05, RB-06.

Those four decide whether the architecture holds:

- **RB-03** — if `AICallable` tools cannot carry our envelope cleanly from an
  out-of-tree plugin, ADR-0002 needs revising and everything shifts.
- **RB-05** — if Blueprint graphs cannot be reconstructed from JSON, the project's
  central promise is reduced to inspection plus patching. Better to know in week one.
- **RB-06** — if `FileSandbox` does not cover package saves, atomic multi-asset
  batching does not work as designed and ADR-0005 needs rework.
- **RB-02** — until this exists, every domain workstream risks rebuilding an Epic tool.

Everything else follows.

## A note on effort

These briefs are not literature reviews. The highest-value action for most questions
here is **opening a header, or running five lines in the editor's Python console**,
not searching the web. `[VERIFIED-RUNTIME]` in an afternoon beats `[DOCS]` in a week.
