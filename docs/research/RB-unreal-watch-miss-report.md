# RB — Unreal Watch missed “editor closed”

**Audience:** UEREMCP / REAgentTools owner  
**Date:** 2026-07-31  
**Session:** Northridge / MMORPG fieldtest (`ueremcp_fieldtest`)  
**Watcher source:** `C:\Users\WhiteWidow\Documents\GitHub\REAgentTools\Optional\UnrealWatchMCP\` (v0.3.0 local, uncommitted vs `10d98cd`)  
**Cursor server id:** `user-unreal-watch` (config key `unreal-watch`)

---

## Verdict (blunt)

**The watcher did not “miss” a closed editor — it was never callable in Cursor this session.**  
`user-unreal-watch` stayed in `serverStatus=error` (“failed during live tool discovery”). Agents hammered `user-unreal-mcp`, which hung/timed out against a dead `:8000` while the `:8001` proxy kept saying “call unreal-watch.check_unreal” — advice that could not be followed.

Even if Cursor discovery were fixed, **the product is not a background watcher**: it is a **pull-only** stdio MCP. Nothing notifies multitask agents when Unreal exits. And `check_unreal`’s `agent_instruction` currently **lies** when the editor is offline (`unreal_running: false` still says “Editor watch clear — Unreal MCP may be used”).

---

## 1. Claimed behavior vs actual behavior

### Claims (README + tool description)

| Claim | Reality |
|-------|---------|
| Host-side detector for freezes/dialogs when Unreal MCP times out | **True in code** (`watch.check_unreal`) — Win32 enum + HTTP probes; no game thread |
| Detect Unreal process up/down | **Partial** — `find_unreal_pids` only sees processes with **visible** top-level windows (`EnumWindows`), not raw process table |
| Detect blocking Slate/Win32 dialogs | **True when called** — owned `UnrealWindow` + `#32770`; `modal.blocking` only if probes also fail |
| Ensure anti-thrash proxy `:8001` | **True when called** — identity-aware; does not kill/rebind |
| “Watch” dialogs / editor close for agents | **False** — no background loop, no MCP notifications, no Cursor push to multitask agents |
| Fail-fast `editor_offline` | **False** — no such field; offline path sets advice correctly but **`agent_instruction` says clear** |
| Available in Cursor as MCP tools | **False this session** — discovery never reaches `connect_success` |

### What `check_unreal` actually returns (fields)

- `unreal_running`, `processes[]`
- `modal.{present,blocking,count,dialogs[]}`
- `editor_responsive`, `mcp_probe`, `proxy_probe`, `http_proxy`, `rc_probe`
- `likely_blocked`, `advice[]`, `agent_instruction`, `auto_action`, optional `alert_path`

**Not returned:** `editor_offline`, subscribe/heartbeat tokens, or any event stream.

### Mode

Configured `UNREAL_WATCH_MODE=report` → detect only; does not auto-dismiss. Still requires an agent to **call** `check_unreal`.

---

## 2. Exact reasons it didn’t catch closed Unreal this session

Ordered by impact:

### R1 — Cursor MCP server stuck in error (primary)

**Evidence (live):**

```text
GetMcpTools(user-unreal-watch) →
  serverStatus: "error"
  serverError: "This MCP server failed during live tool discovery. Its tools are unavailable until the connection is fixed."
  tools: [mcp_auth only]
```

**Evidence (logs):**  
`%APPDATA%\Cursor\logs\20260731T143511\mcp-server-user-unreal-watch.log`

- Endless `connection:connect_start` + process spawn
- Stderr banner `[unreal-watch] start pid=…` (logged as `[error]` because Cursor tags all stderr)
- **Never** `Successfully connected to stdio server`
- **Never** `connection:connect_success` in this Cursor session

Contrast: `user-blender` / `user-unreal-mcp` reach `connect_success` in the same session.

Contrast history:

| When | Watch FSM |
|------|-----------|
| 2026-07-29 | `connect_success` (tools worked) |
| 2026-07-30 evening | `Connection failed: Connection closed`; wrong spawn path once hit `visualtest\.cursor\unreal_mcp_proxy.py` and crashed on mojibake `UNREAL_MCP_REQUEST_TIMEOUT` |
| 2026-07-31 (this fieldtest) | Spawn + hang/retry; **no connect_success all day** |

Field report already recorded this: `docs/MCP_Field_Report_Northridge.md` — `user-unreal-watch` error, **0 successful watch calls**, ~20 mentions only.

**Consequence:** Agents could not invoke `check_unreal` / `dismiss_dialog` via Cursor MCP. “Watcher” was a dead menu entry.

Manual CLI **does** work (same `server.py --check`) — so host logic is not dead; **Cursor stdio discovery is**.

### R2 — Not a watcher; pull-only (structural)

Nothing polls Unreal close in the background. No MCP `notifications/…` to the agent runtime. Editor exit is invisible unless something calls `check_unreal` (or reads `Saved/REAgentTools/modal_alert.json`, which is only written **on** `check_unreal`).

Multitask agents were never “subscribed.”

### R3 — Agents not hooked to use it (and GetStarted tells them not to)

- Subagent prompts said “also check `user-unreal-watch` if useful”; discovery returned error/loading → agents fell through to Win32/Shell or kept retrying Unreal MCP.
- Backlog MCP-013 / GetStarted guidance already lists `do_not_use: ["user-unreal-watch"]` because the server is known dead.
- Proxy upstream errors explicitly say call `unreal-watch.check_unreal` — circular when watch MCP is broken.

### R4 — Even a successful call would not fail-fast cleanly (API bug)

Simulated closed editor (`processes=[]`, ports down):

| Field | Value |
|-------|-------|
| `unreal_running` | `false` |
| `advice` | `UnrealEditor process not found — start the editor.` |
| `likely_blocked` | `false` |
| `agent_instruction` | **`Editor watch clear — Unreal MCP via :8001 may be used…`** |
| `editor_offline` | **absent** |

So the one field agents are told to obey (`agent_instruction`) **contradicts** `unreal_running` / `advice` when the editor is absent.

### R5 — Process detection caveat (secondary)

`find_unreal_pids` uses visible-window enumeration only. Fully closed editor → correctly empty. Headless / no-HWND edge cases could false-negative “running,” but that is **not** this incident (editor was gone; MCP `:8000` refused connections — see `user-unreal-mcp` log WinError **10061** at 16:54).

### R6 — Config drift (minor this session)

`UNREAL_WATCH_PROJECT` points at `…\visualtest`, not `ueremcp_fieldtest`. Only affects `alert_path` / project-local proxy lookup. Does not explain discovery failure. Jul 30 logs show prior config thrash between GitHub and `visualtest\Plugins\…` paths.

---

## 3. Tool list + callable status

| Tool | Schema (summary) | Callable via Cursor MCP now? | Callable via CLI / direct stdio? |
|------|------------------|------------------------------|----------------------------------|
| `check_unreal` | no args — full host report | **No** (server error) | **Yes** (`python server.py --check`) |
| `dismiss_dialog` | `choice`, optional `hwnd` | **No** | Yes (if dialogs present) |
| `get_watch_config` | no args | **No** | Yes (via tools/call on stdio) |
| `set_watch_config` | `mode`, optional `auto_allowlist` | **No** | Yes |
| `mcp_auth` | Cursor stub only | Present when error | N/A |

### Live CLI probe (editor open, 2026-07-31 ~22:49Z)

`server.py --check` returned:

- `unreal_running: true` — two `UnrealEditor` windows (`ueremcp_fieldtest`, `Unreal Engine 5.8`)
- `modal.blocking: false`, `likely_blocked: false`
- `mcp_probe` `:8000` responded (405 on GET `/mcp` — expected)
- `proxy_probe.ok: true` (proxy 1.5.0, upstream `:8000`)
- `agent_instruction`: clear / use `:8001`

### Direct stdio handshake (outside Cursor)

Initialize → `notifications/initialized` → `tools/list` → `ping` → `get_watch_config` → `check_unreal` **succeeded** with Python 3.9. Custom framing works when the client speaks MCP stdio correctly.

**Cursor never completes that handshake** for this server in current sessions (no `connect_success`). Root cause of Cursor-side failure is not fully isolated in one stack frame; evidence points to fragile custom stdio server + reconnect thrash since ~Jul 30, not “wrong Unreal process name.”

Fragile bits in `server.py`:

- Hand-rolled Content-Length reader; `json.loads` on body **not** wrapped — one bad frame **kills the process**
- Stderr startup line (harmless for Blender-like servers, but noise)
- Local uncommitted 0.3.0 changes (protocol echo, prompts/resources list, proxy ensure) vs last committed 0.1.0 that **did** connect on Jul 29

---

## 4. Would closed Unreal yield a clear `editor_offline`?

**No.** Today you get:

```json
{
  "unreal_running": false,
  "likely_blocked": false,
  "advice": ["UnrealEditor process not found — start the editor."],
  "agent_instruction": "Editor watch clear — Unreal MCP via :8001 may be used. …"
}
```

Required for fail-fast:

```json
{
  "status": "editor_offline",
  "unreal_running": false,
  "likely_blocked": true,
  "agent_instruction": "STOP. Unreal Editor is not running. Do not retry Unreal MCP. Ask user to launch the project."
}
```

---

## 5. Recommended fixes

### A. Make Cursor discovery reliable (P0)

1. Replace hand-rolled stdio with **official `mcp` Python SDK** stdio server (match Blender pattern), **or** harden framing with try/except around reads and never exit on one bad JSON.
2. Remove / gate stderr banners if they confuse ops; keep crash traces only.
3. Pin a single install path in `%USERPROFILE%\.cursor\mcp.json` (GitHub `REAgentTools\Optional\UnrealWatchMCP\server.py`); stop alternating with `visualtest\Plugins\…` or proxy wrappers.
4. Acceptance: Cursor log must show `Successfully connected to stdio server` + `GetMcpTools` `serverStatus=ready` with four real tools.

### B. Fix offline / blocked semantics (P0)

In `watch.check_unreal`:

1. Add `status` enum: `ok | editor_offline | modal_blocked | ports_wedged | proxy_unhealthy`.
2. If `not unreal_running` → `status=editor_offline`, `likely_blocked=true` (or separate `abort_mcp=true`), rewrite `agent_instruction` to **STOP**.
3. Never emit “Editor watch clear” unless `unreal_running and editor_responsive and not modal.blocking`.
4. Prefer process snapshot (`CreateToolhelp32Snapshot` / `psutil`) **plus** window enum so “running but no HWND yet” is distinct from offline.

### C. Agent instruction hooks (P0 product)

1. `user-unreal-mcp` / GetStarted: if watch ready, **mandate** `check_unreal` once before retry loops on timeout / 10061 / empty upstream.
2. If watch **not** ready, GetStarted must say `unavailable` and give a Shell one-liner:  
   `python …\UnrealWatchMCP\server.py --check`
3. Remove premature `do_not_use` once discovery is green; until then be honest: “broken, use CLI.”
4. Cursor rules / subagent prompts: on any Unreal MCP timeout → call watch once → branch on `status`.

### D. Optional true “watch” (P1)

- Background thread writing `modal_alert.json` + mtime heartbeat, **or**
- MCP resource that agents can poll cheaply, **or**
- Cursor automation/hook that runs `--check` on MCP idle timeout  

Do not claim “watcher” until one of these exists.

### E. Config hygiene (P2)

- Set `UNREAL_WATCH_PROJECT` to the active project (`ueremcp_fieldtest` for this work).
- Keep `UNREAL_MCP_PROXY_SCRIPT` pointing at canonical proxy; never register the proxy script as the watch MCP command (Jul 30 footgun).

---

## 6. Acceptance tests

### AT-OFFLINE-01 — Editor not running

1. Quit all `UnrealEditor*`.
2. Leave `:8001` proxy up or down (both cases).
3. Run `python server.py --check` and, when fixed, Cursor `check_unreal`.

**Expect:**

- `unreal_running === false`
- `status === "editor_offline"` (after fix)
- `agent_instruction` contains `STOP` / `not running` and does **not** contain `watch clear` / `may be used`
- `likely_blocked === true` **or** explicit `abort_unreal_mcp === true`

### AT-OFFLINE-02 — Cursor discovery

1. Reload MCP / restart Cursor.
2. `GetMcpTools(user-unreal-watch)`.

**Expect:** `serverStatus=ready`; tools include `check_unreal`, `dismiss_dialog`, `get_watch_config`, `set_watch_config`.  
Log contains `Successfully connected to stdio server`.

### AT-DIALOG-01 — Blocking dialog

1. With editor open, force a modal that stalls game thread (e.g. known Slate message box / compile error dialog).
2. Confirm Unreal MCP call times out or empty upstream.
3. `check_unreal`.

**Expect:**

- `modal.present === true`, `modal.blocking === true`
- `editor_responsive === false`
- `likely_blocked === true`
- `agent_instruction` tells agent to `dismiss_dialog` or ask user — not to spam MCP

### AT-DIALOG-02 — Non-blocking floating tab

1. Open Message Log / Output Log only; MCP still answers.
2. `check_unreal`.

**Expect:** `modal.present` may be true; `modal.blocking === false`; `editor_responsive === true`; instruction allows one batched MCP call.

### AT-DIALOG-03 — dismiss_dialog

1. From AT-DIALOG-01, `dismiss_dialog(choice="cancel"|"accept")` as appropriate.
2. Re-check: `modal.blocking === false` and MCP ping succeeds.

### AT-CLOSE-EVENT-01 — (P1 if background watch ships)

1. Start watch heartbeat / alert writer with editor open.
2. Kill editor.
3. Within N seconds, `modal_alert.json` (or resource) shows `editor_offline` without an agent tool call.

**Today this test must fail** — documents the gap.

### AT-AGENT-HOOK-01

1. With watch ready and editor closed, run an agent that only has Unreal MCP + watch.
2. First Unreal call fails (10061 / timeout).

**Expect:** agent calls `check_unreal` once and stops with “editor offline,” no multi-minute retry loop.

---

## 7. Evidence index

| Item | Path / note |
|------|-------------|
| MCP config | `%USERPROFILE%\.cursor\mcp.json` → `unreal-watch` → GitHub `UnrealWatchMCP\server.py` |
| Source | `REAgentTools\Optional\UnrealWatchMCP\{server,watch,config,README}` |
| Cursor watch log | `%APPDATA%\Cursor\logs\20260731T143511\mcp-server-user-unreal-watch.log` |
| Unreal MCP refusals while closed | same session `mcp-server-user-unreal-mcp.log` — WinError 10061 + idle timeouts; message cites `unreal-watch.check_unreal` |
| Prior field note | `ueremcp_fieldtest\docs\MCP_Field_Report_Northridge.md` — watch error, 0 calls |
| Backlog | `MCP_Backlog_API_Shapes.md` MCP-013 |
| Transcripts | fieldtest agent-transcripts — watch loading/error; unstick agent noted watch unavailable |

---

## 8. Summary table

| Question | Answer |
|----------|--------|
| Did watch “miss” closed Unreal? | **No — it was offline in Cursor** |
| Does logic detect closed Unreal when invoked? | **Mostly yes** (`unreal_running: false`) but **`agent_instruction` is wrong** |
| Clear `editor_offline`? | **No** |
| Background dialog/close watch? | **No** |
| Fix priority | Discovery (Cursor) + offline instruction rewrite + agent hook |

---

## 9. Fixed (2026-07-31)

**Repo / branch:** `REAgentTools` → `cursor/unreal-watch-mcp-fix` (UnrealWatchMCP v0.4.0)  
**Commits:** REAgentTools `956c88a`; this report `9e659cf` (UEREMCP `ws-11-northridge-remaining-impl`)  
**Cursor config:** `%USERPROFILE%\.cursor\mcp.json` → spawn via `uv run --python 3.11 --with mcp>=1.9,<2` + GitHub `UnrealWatchMCP\server.py`; `UNREAL_WATCH_PROJECT` → `ueremcp_fieldtest`; `UNREAL_WATCH_HEARTBEAT_S=5`.

### What changed

1. **Cursor discovery** — Replaced fragile hand-rolled stdio as the primary path with official **`mcp` FastMCP** stdio (same family as blender-mcp). Hardened legacy framing kept as ImportError fallback. Verified live: `GetMcpTools(user-unreal-watch)` → `serverStatus=ready` with `check_unreal`, `get_editor_status`, `wait_for_editor`, `dismiss_dialog`, config tools.
2. **Honest offline** — `check_unreal` now returns `status=editor_offline`, `abort_unreal_mcp=true`, `likely_blocked=true`, and `agent_instruction` starts with **STOP** / “not running”. Never emits “watch clear / may be used” when the editor is down. Other statuses: `ok | modal_blocked | ports_wedged | proxy_unhealthy`.
3. **Process detection** — `CreateToolhelp32Snapshot` + window enum (headless / no-HWND no longer false-offline when process exists).
4. **Pull improvements** — New tools `get_editor_status`, `wait_for_editor`; optional heartbeat thread writes `Saved/REAgentTools/modal_alert.json` when `UNREAL_WATCH_HEARTBEAT_S` + project path set.
5. **Tests** — `Optional/UnrealWatchMCP/test_offline_semantics.py` (AT-OFFLINE-01 semantics).

### How to verify in Cursor

1. Command Palette → **MCP: Restart Servers** (or reload window) if status is still error from an older session.
2. Confirm `user-unreal-watch` tools list includes `check_unreal` / `get_editor_status`.
3. With editor open: call `check_unreal` → expect `status=ok`.
4. Quit Unreal: call `check_unreal` → expect `status=editor_offline`, instruction contains `STOP`, no “watch clear”.
5. CLI: `python …\UnrealWatchMCP\server.py --check` (stdlib; no mcp package required).

### Remaining limitations

- Still **not** a Cursor push notifier — heartbeat updates a file; agents must poll tools or the alert JSON.
- GetStarted / proxy “do_not_use watch” backlog copy may still be stale until those docs are updated separately.
