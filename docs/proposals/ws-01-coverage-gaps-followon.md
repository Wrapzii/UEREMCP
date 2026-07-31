# Follow-on: remaining coverage + Tier-1 publish gaps

**Branch:** `ws-01-coverage-gaps-followon`  
**Forked from verified tip:** `7745d3ba31102e5bcb8c1e797e8d9650702802a3`  
**Usable tip:** keep `main` / `UEREMCP-deploy-main` at that SHA (or later verified tips).  
Do **not** block agents on this branch.

## Why this branch exists

The integration tip is deployable: Environment + Core bootstrap + Systems +
capture are live; GeometryScript/Water enabled; ResolveIntent score-gate live.
Remaining work would churn the tip while agents can already use it.

## Queue (from dirty-root BACKLOG, in order)

1. **1.2a / 1b.1** — Override `FToolset::GetJsonSchema()` so live describe returns
   nested `specification` schemas from `schemas/domains/**` (not only
   `{requestJson:string}`). Highest remaining call-count lever.
2. **1.3b** — Template **authoring** (`PromoteToTemplate` write path,
   `CreateTemplate`/`UpdateTemplate`). Empty library is correct; authoring must work.
   Owner: WS-15 — propose if landing elsewhere.
3. **1b.3–1b.7** — existing-assets preference, richer Niagara contract, terminal
   capture, goal-level envelope, response hygiene / proxy banners.
4. **Tier 4 / COVERAGE_PLAN Part IV–V** — UI, mesh composition, physics, data,
   import, lighting, AI — new domains only after the above; Environment/Systems
   already on the verified tip.

## Non-negotiables

- Never claim a tool without live `list_toolsets`.
- Focus mode stays disabled until 1.2a is live-verified alongside rejection echo.
- Before commits: `check_tool_names.py`, `gen_focus_config.py --check`.
- No push from this agent session; dirty root worktree untouched.
