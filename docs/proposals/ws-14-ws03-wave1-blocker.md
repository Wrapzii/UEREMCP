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

**Accepted — Critical Wave 1 gate.** WS-03 has uncommitted work in `UEREMCP-ws03`;
that must be committed and finished before any Wave 2 implementation.

Required, in order:
1. Commit current Core compile progress; restore/register modules honestly.
2. Register `UeremcpTransport` and `UeremcpValidation` in `UEREMCP.uplugin` once
   sources are present (merge from `ws-01-orch` / workstream branches as needed).
3. `[VERIFIED-RUNTIME]` Ping/Echo via MCP `call_tool`, or exact negative finding.
4. Capture RB-03 q6 schema verbatim.

R-04 / R-15 remain open until this lands. Wave 2 implementation stays gated.
