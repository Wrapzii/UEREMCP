# WS-06 → WS-11: run A6 editor proof and add full POC A harness

- **From:** WS-06
- **To:** WS-11
- **Date:** 2026-07-30
- **Orchestrator base:** `e3e3033`

## Ready after merge

Run:

```powershell
tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter UeremcpBlueprint.Toolset.PocA6Reread
```

The new WS-06 automation test:

1. creates and cleans a real scratch Blueprint under `/Game/__UeremcpPoc/`;
2. reads the complete graph;
3. submits externally changed DSL that inserts `Branch -> PrintString`;
4. requires `modified_and_validated` and `validation.reread_after_write: true`;
5. programmatically re-reads and asserts the event, branch, function call, and both
   execution links;
6. checks unchanged-replace hash identity and a second identical no-op with compile
   skipped.

The writer composes Epic `BlueprintTools.write_graph_dsl`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintEpicBridge.cpp:110-129]`
and performs its complete re-read through `FUeremcpBlueprintGraphReader::ReadGraph`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintGraphWriter.cpp:860-895]`.

## WS-11-owned residual

Please add/run the full MCP-facing A1–A11 scenario in `tests/**`, including:

- actual MCP round-trip count (A9), internal operations, wall-clock time, and tokens;
- primitive-call baseline;
- one scenario-level result rather than treating this editor filter as overall POC A;
- cleanup confirmation for `/Game/__UeremcpPoc/`;
- evidence log path and orchestrator tip.

The current WS-11 safe-path mirror rejects `/Game/__UeremcpPoc/`; either authorize
that acceptance root for this named harness or rely on the WS-06 test's scoped cleanup.
Do not broaden deletion policy silently.

## Explicit non-claims

Until this filter is merged into the orchestrator junction and passes in the RE editor,
A6 remains **not proven at runtime**. This proposal does not claim A9, overall POC A,
restart survival, or POC E metrics.

The RE/VisualTest plugin junction must remain on
`UEREMCP-ws01\Plugins\UEREMCP`; do not retarget it to the WS-06 worktree.
