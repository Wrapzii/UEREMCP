# WS-11 POC-B current-lineage evidence handoff

**Tested tip:** `d3f6be92731af3d8ba522f75a060d79580fd73a5`  
**Bundle:** `tests/integration/_logs/poc_b_current_lineage_e249841.json`  
**Status:** criterion evidence assembled; overall POC-B is **not claimed**

The bundle closes the acceptance-gap audit's P0-3 orchestration gap by indexing the
freshest available B1-B10 evidence on one lineage. It does not close the primitive
baseline. Current-run MCP transport was unavailable with `WinError 10061`, so B1 is
an honest transport **SKIP**; the successful one-invocation editor proof is labeled
as an editor equivalent, not MCP proof.

| Criterion | Status | Primary evidence |
|---|---|---|
| B1 | SKIP (transport) | `mcp_poc_b_transport_attempt_20260730_0957.json`; editor equivalent `...FireballInlineMaterials_20260730_100132.log` PASS |
| B2 | PASS | fresh fireball editor assertions |
| B3 | PASS | fresh fireball gate `B3=PASS` |
| B4 | PASS | fresh renderer/material binding assertions |
| B5 | PASS | fresh fireball gate `B5=PASS` |
| B6 | PASS (editor compile-await) | fresh fireball gate `B6=PASS`; no current MCP timing claim |
| B7 | PASS | fresh `SixEmitterGateScaffold` create/reread honesty gate |
| B8 | PASS | separate Create/Verify editor processes and matching ten-asset checkpoint |
| B9 | PASS | fresh manifest present/complete assertions |
| B10 | PASS | production warm-pixel gate: 30,454 warm pixels, 412 particles, six runtime emitters; PNG supplementary |

Metrics use the WS-14 close at `e249841`: `mcp_round_trips=1`,
`internal_operations=46`, and measured editor-equivalent wall clock
`31.370670s`. Tokens are unavailable because the caller exposes no per-call usage.
The primitive baseline is unavailable because the current sequence does not fix the
inputs required for a semantically equivalent executable trial.

## Harness repair

The B8 C++ filter emits pretty-printed JSON after `UEREMCP_POC_EVIDENCE=`. The
existing parser accepted only same-line compact JSON and therefore rejected a
successful Create phase as “evidence marker missing.” WS-11's parser now uses
`JSONDecoder.raw_decode` from each marker and has a multiline regression test.
The strict bundle validator requires B1-B10, a full tested-tip SHA, evidence paths,
honest `pass`/`fail`/`skip` statuses, and `overall_poc_b_claimed: false`.

## Remaining gaps

1. Re-run B1 through live MCP when transport is available.
2. Capture total agent tokens only with a usage-reporting caller.
3. Run the primitive baseline only after WS-07 supplies fixed executable inputs.

No overall POC-B claim is made.
