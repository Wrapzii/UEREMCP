# WS-01: Target project correction — Visual Test, not RE

**Date:** 2026-07-30  
**Status:** Accepted for this session (user correction)  
**Owner:** WS-01 orchestration

## Summary

Agents were incorrectly treating `$UEREMCP_LEGACY_PROJECT`
as the live target. The user did **not** have RE open. Agents opening or modifying maps
under RE caused wrong-map edits and failed live acceptance.

**Canonical live target for all MCP/editor work:** the **Visual Test** project the user
currently has open in Unreal Editor.

| Field | Value |
|---|---|
| Project name | Visual Test (`visualtest`) |
| `.uproject` path | `$UEREMCP_PROJECT/visualtest.uproject` |
| Engine | UE 5.8 (`5.8.0-55116800+++UE5+Release-5.8`) |
| Editor process | `UnrealEditor.exe` pid 23900 (verified 2026-07-30 ~22:28 EDT) |
| MCP | **Live** — `127.0.0.1:8000` LISTENING on editor pid 23900 |
| Loaded map (read-only) | `/Game/__UeremcpPoc/MountainRiverRain/L_MountainRiverRain` |

## RE is explicitly wrong

Do **not**:

- Launch `RE.uproject`
- Open, load, or save maps under RE `Content/`
- Retarget RE's plugin junction for live testing
- Run live MCP acceptance against RE
- Assume `AGENTS.md` / `GROUNDED_FACTS.md` `$PROJ` paths apply to live work

RE remains a historical reference in older docs and test-script defaults. It is **not**
the operator's open project.

## Prior documentation conflict

Several repo files still name RE as the target:

- `AGENTS.md` line 13
- `docs/GROUNDED_FACTS.md` `$PROJ`
- `tests/run_*.ps1` default `-Project` parameter
- `schemas/envelope/request.schema.json` example path

Live-session evidence (`RB-05`, `BENCHMARK_PROTOCOL.md`, `VISUAL_CAPTURE_PROTOCOL.md`,
`BACKLOG.md` §1b.5) already used **visualtest**.

## Closure (2026-07-30 evening)

Applied on deploy-main + workspace:

- `AGENTS.md` Target project → visualtest (no RE live target)
- `docs/GROUNDED_FACTS.md` `$PROJ` → visualtest
- `.claude/settings.json` allow/deny retargeted to visualtest; RE paths denied
- Cursor `mcp.json` (global + visualtest) already pointed at `:8001` + visualtest watch
- Live proxy `:8001` process uses `visualtest/Plugins/REAgentTools/Optional/UnrealMcpProxy/`

**Binary blocker found:** `UnrealEditor-UeremcpValidation.dll` was last linked against
`UnrealEditor-RE.dll` (built on RE). On visualtest boot: `Missing import: UnrealEditor-RE.dll`
→ whole UEREMCP plugin fails → Environment/Systems never stay loaded. Rebuild Validation
against `visualtest.uproject` required.

## Agent contract (effective immediately)

1. **One open editor only** — use the running Visual Test instance; never spawn a second
   editor and never open RE.
2. **One plugin junction** — `visualtest/Plugins/UEREMCP` → `UEREMCP-deploy-main`
   (see `ws-01-visualtest-single-plugin-source-2026-07-30.md`).
3. **One SHA for builds** — `UEREMCP-deploy-main` @ `37b7bf3` until a newer deploy tip
   is explicitly promoted.
4. **Read-only map policy** — do not load/save maps unless the user requests it.
5. **Static tests** in git worktrees are fine; live MCP/editor validation uses Visual
   Test only.

## Worker handoff — stop RE usage

The following in-flight workers were scoped to RE and must **not** continue against RE:

| Worker | ID | RE-scoped behavior to stop |
|---|---|---|
| Deploy consolidation | `d21ef41a` | Rebuild against RE, retarget RE junction, live verify on RE |
| v2 environment verify | `5ea99c8e` (referenced in consolidation prompt) | SnowIceHail dry-run on RE |

**Replacement scope:** git merges and static validation in worktrees; live verify on
Visual Test only, after editor restart loads deploy-main DLLs.

## Plugin junction fix (2026-07-30)

| | Before | After |
|---|---|---|
| `visualtest/Plugins/UEREMCP` | `UEREMCP-ws01/Plugins/UEREMCP` (`b84397f`) | `$UEREMCP_DEPLOY/Plugins/UEREMCP` (`37b7bf3`) |

**Note:** The editor was already running with **old ws01 DLLs** loaded in memory.
Junction retarget does not hot-reload modules. User must restart the editor (or trigger
Live Coding rebuild from deploy-main) before live acceptance sees Environment v2, nested
schemas, or Tier 1b template tools.

## RE junction (unchanged — do not use)

`RE/Plugins/UEREMCP` → `$UEREMCP_DEPLOY/Plugins/UEREMCP` (read 2026-07-30).
Left as-is; agents must not open RE to test it.
