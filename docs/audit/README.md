# Baseline Capability Audits

**Owner:** WS-02. Other workstreams contribute rows via `docs/proposals/`.

## Why this directory gates the project

Epic ships **27 domain toolsets** covering Niagara, GAS, gameplay tags, PCG, StateTree,
UMG, physics, animation, data registry, game features, automation testing, and semantic
search `[VERIFIED: GROUNDED_FACTS.md §3]`. REAgentTools ships **15 more**, and reached a
~5:1 efficiency gain in real use.

The most likely way this project wastes its effort is R-06: fifteen agents
enthusiastically rebuilding tools that already work. This directory is the control.

`AGENTS.md` rule 2: **before proposing any new primitive, record here what the existing
equivalent does and why it is insufficient.** A tool added without an audit entry is a
defect.

## Files

| File | Contents | Brief | Status |
|---|---|---|---|
| `epic-toolsets.md` | Per-tool inventory of all loaded Epic toolsets | RB-02 | **complete (source); runtime schemas pending** |
| `reagenttools.md` | Per-toolset disposition for REAgentTools' 15 toolsets | RB-15 | **in_progress — execute_editor_batch done** |
| `coverage-assertion.md` | Proof that UEREMCP covers everything marked `supersede` | — | not started |
| `raw/` | Verbatim schema dumps so others can grep without repeating the work | RB-02 | — |

Use `_TEMPLATE-capability-matrix.md` for both matrices.

## What to know before you start

Two things that are easy to get wrong:

1. **`.uproject` is not the whole story.** `RE.uproject` enables only some toolsets by
   name, but `AllToolsets` may pull in others `[VERIFIED: RE.uproject]`. Enumerate what
   **actually registers at runtime**, not what is configured.
2. **Everything currently "known" about specific Epic tool names is second-hand.** Names
   like `NiagaraToolsets.*`, `BlueprintTools`, `MaterialTools`,
   `ProgrammaticToolset.execute_tool_script` come from REAgentTools' documentation and
   are tagged `[UNVERIFIED]` in `GROUNDED_FACTS.md §7.5`. Verify them; do not propagate
   them.

## Two questions worth answering first

Everything else in RB-02 can wait behind these:

- **Does `ProgrammaticToolset.execute_tool_script` exist, and what does it accept?**
  REAgentTools built its whole Niagara batching strategy on it. It may already be the
  batching primitive `execute_plan` should compose rather than replace. WS-05 is blocked
  on this.
- **What Blueprint graph/node tools does Epic already have, and how far do they get?**
  If working node-level authoring exists, WS-06's job becomes composition over it rather
  than building graph writes from scratch — which materially reduces RB-05's scope and
  R-01's severity.
