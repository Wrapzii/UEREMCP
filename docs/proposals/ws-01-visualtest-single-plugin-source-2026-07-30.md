# WS-01: Visual Test — single UEREMCP plugin source

**Date:** 2026-07-30  
**Status:** Junction fixed; editor restart pending  
**Owner:** WS-01 orchestration

## Problem

Multiple git worktrees and junction retargeting caused **different source trees and
binaries** to be built vs loaded:

- Visual Test junction pointed at **stale `UEREMCP-ws01`** (`b84397f`, morning builds).
- RE junction (unused by user) pointed at **`UEREMCP-deploy-main`** (evening builds).
- Consolidation workers rebuilt deploy-main while the open editor still ran ws01 DLLs.
- `UeremcpEnvironment` existed in deploy-main but was **missing** from the ws01 tree
  the editor actually loaded.

This explains missing Environment tools, schema mismatches, and capture output landing
in the wrong project (`BACKLOG.md` §1b.5).

## Audit — plugin locations

| Path | Type | Git SHA / tip | Notes |
|---|---|---|---|
| `visualtest/Plugins/UEREMCP` | Junction | **`37b7bf3`** (after fix) | **Canonical live target** |
| `RE/Plugins/UEREMCP` | Junction | `37b7bf3` | Points to deploy-main; **do not open RE** |
| `$UEREMCP_DEPLOY/Plugins/UEREMCP` | Real dir | `37b7bf3` | **Canonical build source** |
| `UEREMCP-ws01/Plugins/UEREMCP` | Real dir | `b84397f` | **Was** visualtest junction; stale |
| `UEREMCP-coverage-gaps-followon/Plugins/UEREMCP` | Real dir | `14cf146` | Merged into deploy-main |
| `UEREMCP-ws16-environment/Plugins/UEREMCP` | Real dir | `fa49175` | Merged into deploy-main |
| `UEREMCP-composability-audit/Plugins/UEREMCP` | Real dir | `d150267` | Merged into deploy-main |
| Dirty root `UEREMCP/Plugins/UEREMCP` | Real dir | `091d8bf` | WS-11 branch; not for live editor |

## DLL evidence (UnrealEditor-UeremcpCore.dll)

| Location | Size | LastWriteTime | Loaded by editor? |
|---|---|---|---|
| visualtest (was ws01) | 265,728 | 2026-07-30 09:23 | **Yes** (in-memory at fix time) |
| deploy-main | 481,792 | 2026-07-30 22:09 | No (junction was ws01) |
| follow-on `14cf146` | 483,328 | 2026-07-30 22:01 | No |

| Module | ws01 (old junction) | deploy-main |
|---|---|---|
| UeremcpCore | present (09:23) | present (22:09) |
| UeremcpEnvironment | **MISSING** | present (22:11) |
| UeremcpTemplates | present (16:41) | present (22:10) |
| UeremcpNiagara | present (10:50) | present (22:10) |

## Git state — recommended single SHA

**Use `UEREMCP-deploy-main` @ `37b7bf3`**

```
37b7bf3 [WS-01] Merge Tier 1b nested schemas and template authoring
c075c89 [WS-01] Merge P0 composability fixes with ws-16 v2 Environment
fa49175 [WS-16] Fix Printf paren in precipitation Niagara name helper
71324e4 [WS-16] Linearize rain Niagara onto composable v2
d150267 [WS-01] Gate registry snapshot and operation catalog freshness
ddf7346 [WS-01] Enforce BuildEnvironment fallback_policy
14cf146 [WS-01] Publish nested MCP schemas and enable template authoring
```

Commits confirmed **in** deploy-main: `14cf146`, `074575a`, `d150267`, `37b7bf3`.  
`ws01` `b84397f` is an ancestor but **not** the tip — deploy-main is strictly ahead.

Merge is **not complete** for every side branch (e.g. dirty root `091d8bf`, router
worktree `82337de`, remaining-domain `b87916a`), but deploy-main is the most complete
**consolidated** tip available on disk with built DLLs.

## Fix applied

```
BEFORE: visualtest/Plugins/UEREMCP → UEREMCP-ws01/Plugins/UEREMCP (b84397f)
AFTER:  visualtest/Plugins/UEREMCP → $UEREMCP_DEPLOY/Plugins/UEREMCP (37b7bf3)
```

Method: `cmd rmdir` + `mklink /J` (2026-07-30 ~22:29 EDT). RE junction untouched.

## What agents must do going forward

1. **Build only from** `$UEREMCP_DEPLOY`.
2. **Live MCP only against** `visualtest.uproject` (pid 23900 / port 8000).
3. **Never** retarget junctions to ws01, follow-on, or RE without updating this doc.
4. **Never** spawn a second editor or open RE.
5. **After junction change:** user must **restart Visual Test editor** before live
   acceptance — loaded DLLs are still ws01-era until restart.
6. Promote a new SHA by merging into deploy-main, rebuilding there, then updating this
   doc — not by pointing the junction at arbitrary worktrees.

## Open items

- [ ] User restarts Visual Test editor to load deploy-main DLLs
- [ ] WS-01: update `AGENTS.md` / `GROUNDED_FACTS.md` `$PROJ` to visualtest
- [ ] WS-01: update `tests/run_*.ps1` default `-Project` to visualtest
- [ ] Re-run live acceptance (Environment v2, nested schemas, templates) on Visual Test
      after restart — static-only until then
