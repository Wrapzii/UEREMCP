# Proposal: UI domain test updates (WS-11)

- **From:** WS-01
- **To:** WS-11
- **Date:** 2026-08-02

## Ask

Please adopt (or re-author) the `tests/world_doc/test_ui_domain.py` updates that cover:

- `bind_inventory_sheet` / `verify_inventory_toggle` in `UI_ACTIONS`
- `test_capture_screen_umg_path` (FWidgetRenderer + fallback `SCREEN_UMG_CAPTURE_UNSUPPORTED`)
- `test_re_inventory_theme_and_replace_tree`
- `test_bind_and_verify_declared`

WS-01 already applied these so catalog/plan registration stays green; ownership still belongs to WS-11.

## Why

UI fidelity ship (no layout template) added plan actions; acceptance tests must list them.
