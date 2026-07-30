# Capability Matrix — REAgentTools

- **Owner:** WS-02
- **Status:** **SEED ONLY — not started.** Structure and verified layout recorded;
  per-tool inventory and dispositions are the work.
- **Source:** repository inspection of `$RAT` on 2026-07-29, plus the project's own
  `Docs/CAPABILITY_MATRIX.md` (**self-reported — treat as claims to verify**).
- **Brief:** [RB-15](../research/RB-15-reagenttools-migration.md)

## Verified structure

`REAgentTools.uplugin` v1.2.2 — `EditorOnly: true`, `CanContainContent: true`,
`EnabledByDefault: true`, depends on `ToolsetRegistry`.
**No `Source/` directory: Python and content only**, ~6,385 lines under `Content/Python/`
`[VERIFIED: $RAT layout 2026-07-29]`.

Registration: `Content/Python/init_unreal.py` calls `toolsets._registration.register()`,
warning "ToolsetRegistry not available — enable ModelContextProtocol" on failure
`[VERIFIED: init_unreal.py]`.

### 15 toolsets

`actor_workflow` · `anim_workflow` · `asset_workflow` · `batch_workflow` ·
`blueprint_workflow` · `capture_workflow` · `character_workflow` · `context` ·
`dress_workflow` · `level_workflow` · `lighting_workflow` · `material_workflow` ·
`niagara_workflow` · `project_workflow` · `validation_workflow`

### Shared modules

`agent_policy` · `anim_helpers` · `blueprint_helpers` · `component_helpers` · `limits` ·
`logging` · `properties` · `resolution` · `results` · `serialization` · `spawn_helpers` ·
`transactions` · `validation`

### Optional external components

- `Optional/UnrealMcpProxy/` — proxy, purpose to determine
- `Optional/UnrealWatchMCP/` — Slate dialog / lockup detection. **Solves a problem we
  will hit** (R-11); evaluate for reuse rather than rebuild.
- `rc_bridge.py` + `_rc_reagent_exec.py` — Remote Control fallback for when client MCP
  discovery fails.

---

## Matrix — TO BE FILLED BY RB-15

| Toolset | Tool | Purpose | Input | Output | Limitations | Altitude | Disposition | Superseded by | Tag |
|---|---|---|---|---|---|---|---|---|---|
| _empty_ | | | | | | | | | |

---

## Self-reported gaps — these are our thesis

From `$RAT/Docs/CAPABILITY_MATRIX.md` `[UNVERIFIED — the authors' own assessment]`:

| Area | Self-reported state |
|---|---|
| Blueprint graph / node authoring | absent — defers to Epic `BlueprintTools` / `BlueprintNodeTools` |
| `create_or_update_blueprint` | class defaults only, no graph authoring |
| Niagara system / emitter / renderer authoring | absent — defers to Epic, batched via one `execute_tool_script` |
| Master material graph editing | absent — defers to Epic `MaterialTools` |
| GAS ability graph authoring | explicitly out of scope |
| Enemy AI / spawners, dungeon proc-gen | "project architecture missing" |
| `run_map_check`, `get_recent_errors_compact` | partial; API may be unavailable |

**The three biggest holes — Blueprint graph round-trip, Niagara construction, material
graph authoring — are exactly UEREMCP's three highest-value targets.** That is
consistent with this prior work rather than a repudiation of it, and it is worth saying
so plainly: REAgentTools stopped where the Python layer stopped being able to reach.

## Prior art to extract, not rewrite

Flagged for immediate handoff — these save other workstreams real time:

| Artifact | To | Why |
|---|---|---|
| `execute_editor_batch` — 8 allowlisted actions, `$ref` chaining, `dry_run` | **WS-05** | Direct precursor to `schemas/batch/plan.schema.json`, including the `$ref` idea. Blocked on this before finalising the grammar. |
| `Docs/NIAGARA_BATCHING.md` — many Epic Niagara calls in one script, compile once at end | **WS-07** | A working answer to a problem WS-07 is about to solve from scratch. |
| `Docs/BENCHMARK_REPORT.md` + `benchmark_ab_live.json` | **WS-11** | The methodology behind the ~5:1 baseline. Extend it so numbers stay comparable (R-17). |
| `common/results.py`, `limits.py`, `serialization.py` | **WS-05** | Compact-result and limit conventions shaped by real token pressure. |
| `common/transactions.py`, `validation.py` | **WS-11** | Existing transaction and validation conventions. |
| `Optional/UnrealWatchMCP` | **WS-12, WS-04** | Modal-dialog/lockup protection (R-11). |
| `Config/DefaultREAgentTools.ini` | **WS-05** | Tuned limits are hard-won information; find out what failures drove them. |

## Project-specific vs core

`dress_workflow`, `character_workflow`, `lighting_workflow`, and parts of
`anim_workflow` look specific to the RE game rather than general Unreal capability.
RB-15 q4: these probably belong in a project extension layer, not the UEREMCP core —
decide deliberately rather than by default.

## Coexistence and cutover — TO BE FILLED

Can both plugins register simultaneously without tool-name collisions? What must work
before REAgentTools is disabled? (RB-15 q15–16.)
