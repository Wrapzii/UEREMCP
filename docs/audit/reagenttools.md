# Capability Matrix — REAgentTools

- **Owner:** WS-02
- **Status:** **in_progress** — `execute_editor_batch` grammar verified; full 15-toolset
  matrix still pending.
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

## `execute_editor_batch` — verified grammar (RB-15 q6 → WS-05)

**Source:** `$RAT/Content/Python/re_agent_tools/toolsets/batch_workflow_tools.py`
`[VERIFIED: read 2026-07-29]`. Machine-readable dump:
`docs/audit/raw/q-reagenttools-execute-editor-batch.json`.

### Tool signature

| Param | Type | Default | Tag |
|---|---|---|---|
| `operations_json` | `str` | — | JSON array of op dicts `[VERIFIED: batch_workflow_tools.py:67,74]` |
| `dry_run` | `bool` | `false` | `[VERIFIED: batch_workflow_tools.py:68]` |
| `stop_on_error` | `bool` | `true` | `[VERIFIED: batch_workflow_tools.py:69]` |

Returns compact JSON `WorkflowResult` string `[VERIFIED: results.py:30-73]`.

### `$ref` resolution (`_resolve_ref`)

| Rule | Detail | Tag |
|---|---|---|
| Syntax | String starting with `$` → prior step **id** (no braces) | `[VERIFIED: batch_workflow_tools.py:37-48]` |
| Lookup | `step_results[step_id]` from earlier op with matching `"id"` | same |
| Resolution order | Return `"label"` if present, else `"path"`, else error | same |
| Literal | Strings not starting with `$` pass through unchanged | same |

**Example:** `{"id":"a1","action":"spawn_actor",...}` then
`{"action":"set_actor_transform","label":"$a1",...}` resolves label from step `a1`.

Label fields accept aliases: `label` \| `actor_label` \| `name` \| `actor`
`[VERIFIED: batch_workflow_tools.py:51-57]`.

### `ALLOWED_ACTIONS` (exact set of 8)

```
resolve_actor | spawn_actor | set_actor_properties | set_actor_transform |
save_level | compile_blueprint | set_asset_properties | save_asset
```

`[VERIFIED: batch_workflow_tools.py:17-26]`. Any other `action` → `ValueError`.

### Limits and control flow

| Control | Behaviour | Tag |
|---|---|---|
| `BATCH_LIMIT` | **20** ops max (`limits.py` + `DefaultREAgentTools.ini`); exceed → immediate error, no execution | `[VERIFIED: limits.py:11, batch_workflow_tools.py:75-81]` |
| `dry_run` | Skips mutating ops with warnings; **`resolve_actor` still runs** | `[VERIFIED: batch_workflow_tools.py:105-197]` |
| `stop_on_error` | On invalid op or `ResolutionError`/`ValueError`, stop remaining ops (default true) | `[VERIFIED: batch_workflow_tools.py:204-212]` |
| Transaction | Single `ScopedEditorTransaction("RE execute_editor_batch")` wraps all ops | `[VERIFIED: transactions.py:12-14]` |

### Disposition

| Aspect | Disposition | Rationale |
|---|---|---|
| `$ref` grammar, allowlist, `dry_run`, `stop_on_error` | **preserve ideas** → WS-05 `execute_plan` schema | Proven prior art; do not invent incompatible grammar |
| `execute_editor_batch` tool surface | **supersede** → UEREMCP `execute_plan` envelope action | ADR-0003 one-envelope-in/out |
| REAgentTools implementation | **internalise** during migration; retire after cutover | Avoid duplicate agent-facing batch tools |

### Relationship to Epic `execute_tool_script`

| | REAgentTools `execute_editor_batch` | Epic `ProgrammaticToolset.execute_tool_script` |
|---|---|---|
| Input | JSON op array | Python `script` with `run()` |
| Scope | 8 allowlisted RE editor ops | Any registered tool via `execute_tool()` |
| Batching model | Declarative steps + `$ref` | Imperative script orchestration |
| **UEREMCP stance** | Adopt `$ref`/allowlist semantics in `execute_plan` | **Compose, don't rebuild** — plan steps delegate Epic batches here |

Niagara and other Epic domains batch through `execute_tool_script`, not
`execute_editor_batch` `[VERIFIED: NIAGARA_BATCHING.md cited in GROUNDED_FACTS §6.2;
programmatic.py:906-953]`.

---

## Matrix — TO BE FILLED BY RB-15

| Toolset | Tool | Purpose | Input | Output | Limitations | Altitude | Disposition | Superseded by | Tag |
|---|---|---|---|---|---|---|---|---|---|
| `batch_workflow` | `execute_editor_batch` | Allowlisted multi-op editor batch with `$ref` | `operations_json`, `dry_run`, `stop_on_error` | WorkflowResult JSON | 8 actions only; BATCH_LIMIT 20; resolve_actor not dry-run | composite | supersede surface / preserve grammar | `execute_plan` | [VERIFIED: batch_workflow_tools.py] |
| _remaining 14 toolsets_ | | | | | | | defer | | |

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
