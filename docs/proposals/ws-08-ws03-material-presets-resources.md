# WS-08 → WS-03: Bundle element_presets.v1.json for packaged Material module

- **From:** WS-08
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** superseded — WS-08 owns `Plugins/UEREMCP/Resources/Materials/**` (`bb04bb9`). Bundled copy shipped in `Resources/Materials/element_presets.v1.json`.

## Problem

`UeremcpMaterialElementPresetsLoader` resolves
`Plugins/UEREMCP/../../schemas/domains/materials/element_presets.v1.json` in developer
checkouts `[VERIFIED: UeremcpMaterialElementPresetsLoader.cpp]`. Packaged or junction-only
deployments without the repo `schemas/` tree fall back to hard-coded C++ defaults and
report that in `interpretation_notes`.

## Ask

WS-03 may add a staged copy at:

```text
Plugins/UEREMCP/Resources/Materials/element_presets.v1.json
```

Source of truth remains `schemas/domains/materials/element_presets.v1.json` (WS-08).
WS-03 should wire a copy/sync step in plugin packaging or document manual sync until
automation exists.

The loader already probes `Resources/Materials/element_presets.v1.json` as second candidate
after the repo-relative schemas path.

## Acceptance

- RE/orch builds without full monorepo `schemas/` still load element defaults from JSON.
- `test_element_presets_loader.py` continues to pass (schema is authoritative).
- No change to WS-08 owned C++ beyond existing fallback behavior.

## Not in scope

- Shipping `templates/` (WS-15 / WS-03 Templates module owns that pattern).
