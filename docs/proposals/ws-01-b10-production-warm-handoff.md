# WS-01 → WS-07: B10 production warm-signature handoff

- **Date:** 2026-07-30
- **Orch tip when written:** `6145563` (on `ws-11-poc-b10-render`)
- **Status:** harness methodology closed; production fireball still FAIL

## Facts

WS-11 tip `0049153` made B10 observe simulated particles (latent ~1.5s tick,
dark backdrop, particle counts). Results:

| Path | Outcome | changed | warm | particles |
|---|---|---:|---:|---:|
| `/Game/__UeremcpPoc/NS_POCB_Fireball` | FAIL `visible_fire_signature_not_observed` | 5405 | **0** | **185** |
| Known-good canary | PASS `viewport_pixel_delta_with_fire_signature` | 5405 | **22** | 43 |

Artifacts:
- `tests/integration/_artifacts/poc_b10_fireball.png`
- `tests/integration/_artifacts/poc_b10_canary.png`

Full record: `docs/proposals/ws-01-editor-filter-results.md` section
"B10 harness fixed — canary PASS, production still FAIL".

## What this means

1. Do **not** treat "zero particles / completed in 62 ms" as the sole remaining
   root cause. That was partly a no-tick harness artifact from the first B10 run.
2. Production now has **live particles** and still **no warm fire signature**.
3. WS-11 will not soften warm thresholds. Harness canary PASS proves the gate works.
4. WS-07 owns making freshly created fireballs show warm pixels. Engage WS-08 only
   with a verified material-invisibility handoff.

## Useful in-progress local work

Uncommitted WS-07 edits already in this worktree disable
`bAllowSystemStateFastPath` and remove MinimalLightweight template emitters in
`UeremcpNiagaraCreate.cpp` (`PrepareRuntimeScaffold`). Keep that — it addresses
age-zero system completion — but it is not enough once particle count is 185.
