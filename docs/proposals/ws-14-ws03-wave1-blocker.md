# Proposal: WS-03 Wave 1 gate — compiling plugin + MCP-reachable tool

- **From:** WS-14
- **To:** WS-03, WS-01
- **Date:** 2026-07-29
- **Severity:** Critical (blocks all Wave 2)

## Finding

`ws-03-plugin` has **no commits beyond `main`**. `docs/WORK_ALLOCATION.md` Wave 1
requires:

> A compiling `UToolsetDefinition` with one `AICallable` tool reachable from an MCP client

`UEREMCP.uplugin` registers `UeremcpCore` and `UeremcpProtocol` only. WS-11's
`tests/run_editor_tests.ps1` disables `UEREMCP` because the editor aborts when the
plugin is enabled without a built `UeremcpCore`.

WS-01 accepted registering `UeremcpValidation` in the uplugin (`ws-11-register-validation-module.md`)
but assigned implementation to WS-03 — **not done**.

## Ask

1. Land `UeremcpCore` with a reference `AICallable` tool (`ping` or envelope echo) that
   round-trips JSON through ToolsetRegistry.
2. Register `UeremcpValidation` (and later domain modules) in `UEREMCP.uplugin`.
3. Document `[VERIFIED-RUNTIME: ...]` evidence: MCP `call_tool` succeeds against RE project.
4. Remove the `-DisablePlugins=UEREMCP` workaround from the default test runner path once
   the shipping plugin loads.

## Why this blocks others

Without WS-03, ADR-0002 is unproven, editor integration tests run against a probe plugin,
and WS-06 cannot start POC A.

## Response

**Partially closed — 2026-07-29 (WS-01).**

Done:
1. WS-03 landed `595a73d` on `ws-03-plugin` — Core/Protocol/Transport/Validation
   DLLs present; uplugin registers all four.
2. Integrated into `ws-01-orch` as `56f5d36` (Protocol conflicts resolved by
   taking WS-03 UE 5.8 `FSharedString` key fixes).
3. RB-03 q6 schema captured verbatim (bare `type:string` + description).
4. In-editor automation Ping/Echo/Schema previously 3/3 Success.

**Closed — 2026-07-30 (WS-01, R-04):**

MCP on target project RE, shipping UEREMCP only (`-NoProbe`; probe disabled):

| Step | Result | Tag |
|---|---|---|
| Process | `UnrealEditor.exe` → `RE.uproject`, MCP `:8000` LISTENING (pid 38972) | `[VERIFIED-RUNTIME: Get-CimInstance Win32_Process; netstat 2026-07-30]` |
| `list_toolsets` | includes `UeremcpCore.UeremcpReferenceToolset` | `[VERIFIED-RUNTIME: user-unreal-mcp list_toolsets 2026-07-30]` |
| `describe_toolset` | Ping (no args); Echo `requestJson` `type:string` (RB-03 q6) | `[VERIFIED-RUNTIME: describe_toolset UeremcpCore.UeremcpReferenceToolset 2026-07-30]` |
| `call_tool` Ping | `status=no_change_required`, `protocol_version=1.0.0` | `[VERIFIED-RUNTIME: call_tool Ping 2026-07-30]` |
| `call_tool` Echo | `request_id=mcp-echo-r04` echoed, `status=no_change_required` | `[VERIFIED-RUNTIME: call_tool Echo 2026-07-30]` |
| Cmd automation | Ping/Echo/RegisterAndCaptureSchema 3/3 Success | `[VERIFIED-RUNTIME: run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter UeremcpCore.ReferenceToolset]` |

**Operator note:** If Unreal prompts to rebuild **`UeremcpValidationProbe`**, click **No**.
Disable/remove the probe plugin; keep **UEREMCP** enabled. The probe is interim
launch-smoke only — shipping gate is `UeremcpValidation` inside `UEREMCP.uplugin`.

**Also closed:** Shipping `Rollback.MultiAssetDiscard` with `-KeepUeremcp`
`[VERIFIED-RUNTIME: WS-11 bf30d8f]`. R-03 mitigated for Content/ full-Discard.

Wave 2 implementation stays gated until Phase 1 exit (R-01 impl, R-06 runtime).
