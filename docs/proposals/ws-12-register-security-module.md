# Proposal: Register `UeremcpSecurity` in `UEREMCP.uplugin`

- **From:** WS-12
- **To:** WS-03 (owns `Plugins/UEREMCP/UEREMCP.uplugin`)
- **Date:** 2026-07-30
- **Related:** ADR-0010, `docs/SECURITY.md`, `docs/proposals/ws-04-uplugin-module-registration.md`
- **Status:** open â€” module sources on `ws-12-security`; uplugin entry needed

## Ask

Add an Editor module entry for `UeremcpSecurity` to `UEREMCP.uplugin`, and ensure the
plugin target links `UnrealEditor-UeremcpSecurity.dll` alongside the existing modules.

Suggested block (mirror `UeremcpValidation`):

```json
{
  "Name": "UeremcpSecurity",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

**Loading order:** after `UeremcpProtocol` (module already depends on it via
`UeremcpSecurity.Build.cs`). Default phase is fine if Protocol loads first in the
Modules array.

## Why WS-03

WS-12 owns `Plugins/UEREMCP/Source/UeremcpSecurity/**` only. Per AGENTS.md rule 3,
`UEREMCP.uplugin` is WS-03 owned.

## Dispatcher hook (follow-up)

Once registered, WS-03 / `UeremcpCore` should call before domain services:

1. `FUeremcpPermissionPolicy::Evaluate`
2. `FUeremcpPathPolicy::ValidateSoftPath` / `ValidateFilesystemPath`
3. `FUeremcpMutatorQueue::TryAcquire` (when implemented)

## Build verification

After registration and compile:

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Security"
python Plugins/UEREMCP/Source/UeremcpSecurity/scripts/test_security_contract.py
python tools/check_ownership.py --ws WS-12
```

## Out of scope

- No Epic MCP fork for auth.
- No wrapping `ProgrammaticToolset.execute_tool_script` as a UEREMCP tool.

---

## WS-01 response

- **Date:** 2026-07-30
- **Status:** **Accepted**

WS-03: register `UeremcpSecurity` in `Plugins/UEREMCP/UEREMCP.uplugin` after Wave 2 module
sources are on `ws-01-orch` (merge `241c19d` integrated). Module entry:

- **Name:** `UeremcpSecurity`
- **Type:** Editor (`TargetAllowList`: Editor)
- **LoadingPhase:** `Default` (after `UeremcpProtocol` in the Modules array)
## WS-01 follow-up (2026-07-30)

- **Status:** Still **open** — UeremcpSecurity sources on ws-01-orch; **not** in UEREMCP.uplugin after merge 1245fa4. Awaiting WS-03 registration commit.

