# RB-15: REAgentTools disposition and migration

- **Owner:** WS-02
- **Status:** not_started
- **Blocks:** the migration plan; prevents rebuilding working functionality
- **Priority:** high

## Framing

Master prompt §4 is blunt in both directions: **do not blindly rewrite working
functionality**, and **do not preserve weak abstractions merely for compatibility.**

REAgentTools is ~6,385 lines of Python across 15 toolsets, registering into the same
`ToolsetRegistry` we are targeting, with no `Source/` directory
`[VERIFIED: $RAT layout]`. It reached a ~5:1 efficiency gain in real use
(`docs/WHY.md`). It is real, working prior art — treat it as such.

## Questions

### A. Per-toolset disposition

For each of the 15 toolsets — `actor_workflow`, `anim_workflow`, `asset_workflow`,
`batch_workflow`, `blueprint_workflow`, `capture_workflow`, `character_workflow`,
`context`, `dress_workflow`, `level_workflow`, `lighting_workflow`,
`material_workflow`, `niagara_workflow`, `project_workflow`, `validation_workflow` —
determine:

1. What it does, its inputs, outputs, and actual limitations.
2. Disposition: **preserve as-is** / **become an internal primitive** / **replace with a
   goal-level operation** / **retire**.
3. If replaced, which UEREMCP action supersedes it, and is coverage genuinely equal or
   better? Master prompt §12 requires we at minimum match practical capability coverage.
4. Is it project-specific to RE (`dress_workflow`, `character_workflow`,
   `lighting_workflow` look like it), and should that live in a project extension layer
   rather than the core?

### B. Reusable mechanisms

5. `common/transactions.py`, `validation.py`, `results.py`, `serialization.py`,
   `limits.py`, `properties.py`, `resolution.py` — what conventions are worth carrying
   into C++? The compact-result and limit conventions in particular were shaped by real
   token pressure and should inform `response_detail`.
6. `execute_editor_batch` — 8 allowlisted actions, `$ref` chaining, `dry_run`. **This is
   direct prior art for `schemas/batch/plan.schema.json`.** Document its `$ref` grammar
   and its failure semantics; WS-05 is waiting on it before finalising the batch schema.
7. `agent_policy.py` — what policy does it encode, and is any of it still right?
8. `rc_bridge.py` / `_rc_reagent_exec.py` — the Remote Control fallback for when client
   MCP discovery fails. Is that failure mode still real? If so, we inherit it.
9. `Optional/UnrealWatchMCP` — Slate dialog/lockup detection. Hand to RB-04 q17 and
   RB-13 q11; this solves a problem we will certainly hit.
10. `Optional/UnrealMcpProxy` — what problem did it solve, and does ADR-0002 make it
    unnecessary?

### C. Knowledge, not code

11. `Docs/NIAGARA_BATCHING.md` — the documented pattern of batching Epic Niagara calls
    into one `execute_tool_script` with a single compile at the end. **Hand to WS-07 and
    WS-05 immediately**; it is a working answer to a problem they are about to solve
    from scratch.
12. `Docs/BENCHMARK_REPORT.md` + `benchmark_ab_live.json` — the measurement
    methodology behind the ~5:1 baseline. Hand to WS-11 (RB-14 q11).
13. `Docs/RESEARCH.md`, `EXPAND_PLAN.md`, `TEST_REPORT.md`, `VISUAL_LOOP.md`,
    `AGENT_DEFAULTS.md`, `TOOL_CATALOG.md`, `USAGE_GUIDE.md`,
    `REMOTE_CONTROL_MCP.md` — extract findings still valid, flag anything contradicted
    by `GROUNDED_FACTS.md`.
14. `Config/DefaultREAgentTools.ini` — what limits and policies are configured, and were
    they tuned in response to real failures? Tuned limits are hard-won information.

### D. Coexistence

15. Can REAgentTools and UEREMCP run **simultaneously** during migration — do tool names
    collide in the registry, and can `SetNameFilters` separate them cleanly?
16. What is the cutover plan, and what must work before REAgentTools is disabled?

## Deliverables

- [ ] `docs/audit/reagenttools.md` — per-toolset disposition matrix
- [ ] A migration plan with an explicit cutover bar
- [ ] The `execute_editor_batch` `$ref` grammar delivered to WS-05
- [ ] `NIAGARA_BATCHING.md` findings delivered to WS-07
- [ ] Benchmark methodology delivered to WS-11
- [ ] A "do not rebuild — this already works" list for all workstreams
