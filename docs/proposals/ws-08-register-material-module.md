# WS-08 → WS-03: Register UeremcpMaterial in UEREMCP.uplugin

- **From:** WS-08
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** requested (Wave 2 scaffold landed)

## Ask

Add the `UeremcpMaterial` editor module to `Plugins/UEREMCP/UEREMCP.uplugin` and ensure
the target project enables **EditorToolset** (MaterialTools) so material composition
can batch Epic primitives when implementation lands.

### Proposed `.uplugin` module entry

```json
{
  "Name": "UeremcpMaterial",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": ["Editor"]
}
```

Insert after domain modules that load before materials (e.g. after `UeremcpNiagara` if
present, otherwise after `UeremcpProtocol`).

### Plugin dependencies

Confirm or add in `Plugins` array:

```json
{ "Name": "EditorToolset", "Enabled": true }
```

MaterialTools lives under EditorToolset's Python toolsets. If compile fails on missing
`MaterialEditor` module headers, the RE project already enables material editor modules
when the plugin is present — verify `MaterialEditor` is reachable via `UeremcpMaterial.Build.cs`.

## What WS-08 shipped

- `Plugins/UEREMCP/Source/UeremcpMaterial/**` — module + `UUeremcpMaterialToolset`
  (`Echo`, `CreateVfxMaterial` stub)
- `schemas/domains/materials/**` — `create_vfx_material` specification schema

## Registration

`UUeremcpMaterialToolset` follows the same `UToolsetDefinition` + `AICallable` pattern as
`UUeremcpReferenceToolset`. Resolve RB-03 q4 (auto-discovery vs explicit
`RegisterToolset`) when wiring domain toolsets.

Apply `SetNameFilters` to hide internal MaterialTools primitives once batching is live
(ADR-0002 rule 5).

## WS-15 dependency

`templates/niagara/niagara.projectile.elemental.v1.json` construction_plan steps
`core_material` and `trail_material` call `create_vfx_material` with
`purpose: elemental_projectile_core|trail` and `element: {{inputs.element}}`. The schema
and stub are aligned; full execution requires this module registered and MaterialTools
composition implemented.

## Blocker

Until this lands, the Material module **will not compile in the RE project** when copied
into `Plugins/UEREMCP` — Unreal only builds modules listed in `.uplugin`.
