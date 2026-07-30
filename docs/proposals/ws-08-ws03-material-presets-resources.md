# WS-08 → WS-03: Bundle element_presets.v1.json — accepted

- **From:** WS-08
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** accepted (WS-03 implemented staged bundle)

## Response

WS-03 staged a packaged copy at:

```text
Plugins/UEREMCP/Resources/Materials/element_presets.v1.json
```

**Source of truth remains** `schemas/domains/materials/element_presets.v1.json` (WS-08).
The staged file mirrors schema version 2 as of orch sync. When WS-08 updates the schema
file, re-sync this bundle until CI automation exists.

## Loader contract (unchanged)

`UeremcpMaterialElementPresetsLoader::ResolvePresetsJsonPath()` probes, in order:

1. `{PluginBase}/../../schemas/domains/materials/element_presets.v1.json` (developer checkout)
2. `{PluginBase}/Resources/Materials/element_presets.v1.json` (packaged / junction-only)

No WS-08 C++ changes required.

## Acceptance evidence

- Junction-only RE deployments without monorepo `schemas/` load element defaults from JSON.
- `python schemas/domains/materials/test_element_presets.py` remains authoritative against schema path.
- WS-08 `test_element_presets_loader.py` (if present) should pass against bundled path in packaged layout.

## Follow-up (not blocking)

- Add a pre-commit or CI diff check that fails when schema and staged bundle diverge.
- Consider extending `docs/WORK_ALLOCATION.md` to assign `Plugins/UEREMCP/Resources/**` to WS-03.
