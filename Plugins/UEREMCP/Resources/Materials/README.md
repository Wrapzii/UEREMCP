# UEREMCP Material packaged resources (WS-08)

**Owner:** WS-08 (`Plugins/UEREMCP/Resources/Materials/**` per `bb04bb9`).

## Loader resolution order

`UeremcpMaterialElementPresetsLoader::ResolvePresetsJsonPath()` probes in order:

1. **`../../schemas/domains/materials/element_presets.v1.json`** — developer monorepo checkout (authoritative while editing schemas).
2. **`Resources/Materials/element_presets.v1.json`** (this folder) — packaged/plugin-only deployments without repo `schemas/`.
3. **C++ fallback** — when neither file exists; `interpretation_notes` report the fallback.

When both (1) and (2) exist, **(1) wins** so schema edits in `schemas/domains/materials/` take effect immediately in dev without manual sync. CI/offline test `test_resources_materials.py` enforces parity between (1) and (2).

## Sync rule

After editing `schemas/domains/materials/element_presets.v1.json`, copy or sync to `element_presets.v1.json` here before commit. `$comment` fields may differ; payload keys must match.
