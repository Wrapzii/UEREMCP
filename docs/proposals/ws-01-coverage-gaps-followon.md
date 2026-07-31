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

1. **1.2a / 1b.1** — **DONE (live 2026-07-31).** `FUeremcpSchemaPublishingToolset`
   wraps registered Ueremcp handlers; `describe_toolset` publishes nested ADR-0003
   envelope + domain `specification` (e.g. BuildEnvironment.seed). UFUNCTION still
   takes `requestJson`; ExecuteToolInternal accepts nested args or legacy string.
2. **1.3b / 1b.2** — **DONE (live).** `CreateTemplate` / `UpdateTemplate` +
   non-preview `PromoteToTemplate` write under `Saved/UEREMCP/Templates/agent/`.
3. **1b.3** — **DONE.** `existing_assets` + `domain` context filters in ResolveIntent.
4. **1b.4–1b.7** — still open (Niagara contract / terminal capture / goal envelope /
   proxy hygiene). See BACKLOG ledger.
5. **Tier 4** — deferred; Environment/Systems already on tip.

## Live verify notes

- Junction may point at this worktree while verifying; restore to deploy tip or FF
  deploy/main only after operator decision.
- Dev: `mklink /J Plugins\UEREMCP\Content\Schemas schemas` (see Schemas.README.md).

