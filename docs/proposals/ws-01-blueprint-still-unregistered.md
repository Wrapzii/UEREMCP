# WS-01 → WS-03: UeremcpBlueprint still unregistered after Wave 2 merge

- **From:** WS-01 (orchestration)
- **To:** WS-03 (owns `Plugins/UEREMCP/UEREMCP.uplugin`)
- **Date:** 2026-07-30
- **Status:** open
- **Related:** `docs/proposals/ws-06-register-blueprint-module.md`

## Context

`ws-01-orch` merged `ws-03-plugin` at `1245fa4` (`ab94b85` — Echo/Protocol rewire + `UeremcpNiagara` registration). `Plugins/UEREMCP/Source/UeremcpBlueprint/**` is already integrated on orchestration (`8fe896c`), but `UEREMCP.uplugin` **does not** list `UeremcpBlueprint`.

## Ask

Add the `UeremcpBlueprint` editor module entry per `ws-06-register-blueprint-module.md` (same JSON block). Prefer a dedicated WS-03 commit on `ws-03-plugin` so orch can merge without WS-01 editing uplugin.

## Also pending on orch (same merge)

Sources present; uplugin entries still missing — separate proposals accepted, awaiting WS-03:

- `UeremcpSecurity` — `ws-12-register-security-module.md`
- `UeremcpTemplates` — `ws-15-register-templates-module.md`

## Current Modules[] after merge

`UeremcpProtocol`, `UeremcpNiagara`, `UeremcpCore`, `UeremcpTransport`, `UeremcpValidation`

## WS-03 kick

Please land uplugin registrations for Blueprint (and Security/Templates when ready) on `ws-03-plugin`; WS-01 will merge again.
