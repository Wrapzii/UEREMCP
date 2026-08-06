# Proposal: UI fidelity + screen capture (no layout template)

- **From:** WS-01 (default owner of unowned `UeremcpUI/**`)
- **Date:** 2026-08-02
- **Scope:** P0/P1 from UI field diagnosis — **no** Tarkov/mage layout template

## Why

Agents cannot ship player-skin UI: UeremcpUI is solid-color stubs; `capture_ui_frame`
rejects screen UMG; Cursor `mcp.json` pointed at `:8001` proxy while Epic/UEREMCP
listens on `:8000`. Live Tab HUD chrome is RE `NativePaint` + `REInventoryUIStyle`,
not UMG sheets.

## Changes

1. **Transport:** Cursor `mcp.json` `unreal-mcp` → `http://127.0.0.1:8000/mcp`
2. **Theme `re_inventory`:** port `REInventoryUIStyle` color tokens into `FUeremcpUITheme`
3. **Fidelity:** optional image/9-slice brushes + layered gold-rim frames; font face path
4. **Screen capture:** PIE path via screenshot-with-UI / widget draw (no longer hard-reject only)
5. **Patch:** `replace_tree:false` keeps existing WBP and applies theme/layout patches
6. **Bind + verify:** soft-bind inventory sheet class; PIE Tab open/close assert tool
7. **RE:** honor pre-set `UMGSheetClass` instead of always forcing `bAllowUMGShell=false`

## Non-goals

- New collage layout template / single-panel Tarkov mage template asset

## Status (2026-08-02)

Implemented in `UeremcpUI` (toolset `0.2.0-ui-fidelity`):

| Item | Done |
|------|------|
| Theme `re_inventory` + layered gold-rim frames | yes |
| Nested `widgets[]` + brush/9-slice/font paths | yes |
| `replace_tree:false` theme patch | yes |
| `capture_ui_frame` screen path via `FWidgetRenderer` | yes (fallback reject if no widget) |
| `bind_inventory_sheet` + `verify_inventory_toggle` | yes (soft RE reflection) |
| RE `EnsureHUD` soft-bind honor | yes (RE project, outside repo) |
| Layout template | **skipped per user** |

## Test note for WS-11

`tests/world_doc/test_ui_domain.py` updated for new actions + FWidgetRenderer honesty contract.
