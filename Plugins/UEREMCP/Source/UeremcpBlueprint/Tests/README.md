# UeremcpBlueprint tests

## Offline (no editor)

Validates representative `read_graph` JSON fixtures against `schemas/graph/graph.schema.json`:

```bash
python Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/py/run_tests.py
```

Fixtures live under `Tests/fixtures/` and model the P1 reader shape (nodes, pins, links,
diagnostics, fidelity). This is the offline gate for POC A criterion A1 schema conformance.

## Editor automation

Requires RE plugin junction on orch (`UEREMCP-ws01`) after merge — see
`docs/proposals/ws-06-re-runtime-junction-policy.md`. WS-06 does not retarget the RE
junction locally.

- `UeremcpBlueprint.Toolset.ReadGraphRoundTrip`
- `UeremcpBlueprint.Toolset.SubmitGraphValidation` (unchanged replace / revision guard)

Run via WS-11 harness: `tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter UeremcpBlueprint`
