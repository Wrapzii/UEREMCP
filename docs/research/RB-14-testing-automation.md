# RB-14: Testing and automation infrastructure

- **Owner:** WS-11
- **Status:** Wave 1 harness green for unit + interim editor; shipping UEREMCP gate blocked (C-3)
- **Blocks:** every verification claim in the project
- **Priority:** high
- **Updated:** 2026-07-29 (C-3: probe collapsed; shipping blocker documented)

## Framing

`AGENTS.md` rule 6 says success requires verification, and rule 7 says every deliverable
ships with tests. Neither is possible until there is a harness. **This brief blocks the
credibility of every other workstream**, so it runs in Wave 1.

`AutomationTestToolset` is enabled in RE `[VERIFIED: RE.uproject]` — start there.

## Current harness (what landed)

| Path | Purpose | Status |
|---|---|---|
| `python tests/run_unit_tests.py` | Fast out-of-editor unittest discovery under `tests/unit/` | **Green** (9 tests) |
| `pwsh tests/run_editor_tests.ps1 -Filter ...` | UnrealEditor-Cmd + Automation RunTests + Quit | **Green** (~26s cycle for Validation filter) |
| `Plugins/UEREMCP/Source/UeremcpValidation/**` | Editor module: scratch helpers + automation tests | Scaffold; needs WS-03 uplugin registration |
| `tests/integration/editor_plugin/UeremcpValidationProbe/` | Interim **launch-smoke only** (no Rollback body; C-3) | Launch-smoke; junction optional |
| Scratch root `/Game/__UeremcpTests/` + `FUeremcpScratchGuard` | Guaranteed cleanup | Documented + unit-tested + runtime-proven |

### Scratch-path conventions (publish to all workstreams)

1. All test assets under **`/Game/__UeremcpTests/<Suite>/`** only.
2. Never write to `/Game/__UeremcpPoc/` unless the test **is** a POC gate.
3. Use `FUeremcpScratchGuard` (or equivalent) so cleanup runs on failure.
4. Cleanup helpers refuse paths outside the tests root.
5. Never touch real project content.

Helpers: `Plugins/.../UeremcpValidation/Public/UeremcpScratchPaths.h` (and probe copy).

### Editor Cmd blocker

Observed 2026-07-29: Cmd exits during plugin load with  
`Plugin 'UEREMCP' failed to load because module 'UeremcpCore' could not be found`  
before any automation queue runs. `tests/run_editor_tests.ps1` now defaults to
`-DisablePlugins=UEREMCP` and `-EnablePlugins=UeremcpValidationProbe` so WS-11 can
proceed without editing `RE.uproject` or waiting on WS-03's module binaries.
Registration proposal remains: `docs/proposals/ws-11-register-validation-module.md`.

## Questions

1. What automation test frameworks are available — `IMPLEMENT_SIMPLE_AUTOMATION_TEST`,
   `FAutomationTestBase`, functional tests, Gauntlet? Which suit asset-manipulation
   tests? **Answer (partial):** WS-11 uses `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with
   `EditorContext | ProductFilter` for asset/sandbox tests. Epic ToolsetRegistry uses
   DEFINE_SPEC under `AI.ToolsetRegistry.Sandbox.Library`  
   `[VERIFIED: $TR/.../Private/Tests/SandboxLibraryTest.cpp]`. Gauntlet not evaluated.
2. How are tests run — **Yes, unattended:** `UnrealEditor-Cmd.exe` with
   `-ExecCmds="Automation RunTests <filter>; Quit"` via `tests/run_editor_tests.ps1`.
   Human UI optional. **Caveat:** broken Enabled plugins abort startup.
3. Cycle time — **open** (first successful Cmd cycle not timed yet).
4. Scratch path + cleanup — **yes**; see conventions above.
5. Deterministic compile await (BP/Niagara/shader) — **open**
6. PIE from tests — **open**
7. Agent run via `AutomationTestToolset` MCP — **open** (toolset enabled; not wired)
8. Rollback path — joint with RB-06; test implemented, not green yet.
9. Idempotency / revision tests — **not started** (ADR-0006).
10. Out-of-editor unit tests — **yes**, `tests/unit/` + `run_unit_tests.py`.
11. Benchmark harness vs REAgentTools — **not started**.
12. Editor relaunch loop reliability — **open**.

## Deliverables

- [x] Working harness with one passing editor integration test —
      `UEREMCP.Validation.Harness.Smoke` **Success** (and Rollback gate Success)
- [x] Fast out-of-editor unit test path
- [~] `Rollback.MultiAssetDiscard` **green**; `Idempotency.RepeatedCreate` /
      `Revision.StaleRejected` not started
- [x] Scratch-path conventions + cleanup helpers published (this brief + `tests/README.md`)
- [ ] Benchmark harness extending REAgentTools'
- [x] Statement of what **cannot** yet be automatically verified: BP/Niagara/shader
      compile await, PIE, MCP-driven AutomationTestToolset, deletions / Config/Saved
      sandbox coverage, Idempotency/Revision ADR-0006 gates, full UEREMCP.uplugin
      registration path (still on probe + `-DisablePlugins=UEREMCP`)
