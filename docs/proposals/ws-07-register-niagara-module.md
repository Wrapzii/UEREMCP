# WS-07 â†’ WS-03: Register UeremcpNiagara in UEREMCP.uplugin

- **From:** WS-07
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** closed (WS-01, 2026-07-30; uplugin entry landed in merge `1245fa4`)

## Ask

Add the `UeremcpNiagara` editor module to `Plugins/UEREMCP/UEREMCP.uplugin` and ensure
the target project enables **NiagaraToolsets** (and Niagara editor modules) so
`UeremcpNiagara.Build.cs` private dependencies resolve.

### Proposed `.uplugin` module entry

```json
{
  "Name": "UeremcpNiagara",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": ["Editor"]
}
```

Insert after `UeremcpProtocol` (domain modules load after core/protocol).

### Plugin dependencies

Confirm or add in `Plugins` array:

```json
{ "Name": "Niagara", "Enabled": true }
```

NiagaraToolsets is typically pulled by the RE project when MCP Niagara tools are used;
if compile fails on missing `NiagaraToolsets`, add explicit plugin dependency.

## What WS-07 shipped

- `Plugins/UEREMCP/Source/UeremcpNiagara/**` â€” module + `UUeremcpNiagaraToolset`
  (`Echo`, `InspectSystem` stub)
- `schemas/domains/niagara/**` â€” `inspect_system`, `create_niagara_effect`, `graph-ext`

## Registration

`UUeremcpNiagaraToolset` follows the same `UToolsetDefinition` + `AICallable` pattern as
`UUeremcpReferenceToolset`. Resolve RB-03 q4 (auto-discovery vs explicit
`RegisterToolset`) when wiring domain toolsets.

Apply `SetNameFilters` to hide internal NiagaraToolsets primitives once batching is live
(ADR-0002 rule 5).

## Blocker

Until this lands, the Niagara module **will not compile in the RE project** when copied
into `Plugins/UEREMCP` â€” Unreal only builds modules listed in `.uplugin`.

---

## WS-01 response

- **Date:** 2026-07-30
- **Status:** **Closed** — `UeremcpNiagara` in `UEREMCP.uplugin` after merge `1245fa4` (WS-03 `ab94b85`)

WS-03: register `UeremcpNiagara` in `Plugins/UEREMCP/UEREMCP.uplugin` after Wave 2 module
sources are on `ws-01-orch` (merge `c2ee78a` integrated). Module entry:

- **Name:** `UeremcpNiagara`
- **Type:** Editor (EditorOnly / `TargetAllowList`: Editor)
- **LoadingPhase:** `Default`
- **Depends:** `ToolsetRegistry` (via `UeremcpNiagara.Build.cs` private deps; same pattern as other domain toolsets)

Do not add a duplicate `UeremcpEnvelope` / Protocol shim â€” `UeremcpProtocol` already ships
`UeremcpEnvelope.cpp` on orchestration; WS-07 stale note on missing `.cpp` is not actionable.

**Closed (2026-07-30):** Verified UeremcpNiagara and Niagara plugin dependency in Plugins/UEREMCP/UEREMCP.uplugin on ws-01-orch after [WS-01] Integrate WS-03 Wave 2 module registrations (1245fa4).
