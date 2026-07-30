# WS-07 → WS-11: POC B B1 MCP single-call fireball handoff

**Status:** Open — editor parity green on orch `8a8c75d`; MCP transport proof pending  
**Owner:** WS-11 harness + metrics; WS-07 owns request fixture + `poc_b_gates` fields

## Context (orch `29b4b06`)

POC-B blocked on:

1. **B1 MCP transport** — one HTTP/MCP round trip, not editor automation only
2. **B10 visible render** — supplementary evidence (see `ws-07-ws11-poc-b10-visible-render.md`)
3. **Metrics/baseline** — WS-04 envelope `metrics` + WS-11 `poc-metrics.md`

Editor gates already PASS on `8a8c75d`:

- `UEREMCP.Niagara.POCB.FireballInlineMaterials` — B1–B6/B9, all six B4 roles
- `UEREMCP.Niagara.POCB.Restart.Create/Verify` — B8
- B7 scaffold — prior green

## WS-07 fix (2026-07-30)

**`AwaitCompile` MCP crash:** `QueryCompileComplete` on hybrid `ActiveCompilations` crashes during synchronous MCP dispatch (SharedPointer IsValid). **Never call** `PollForCompilationComplete` on live tool dispatch.

**B1/B6 strategy (Option 1 — landed):** Observe-only poll via `UNiagaraExternalEditUtilities::GetSystemCompileState` until script VM `LastCompileStatus` aggregate is UpToDate — no `QueryCompileComplete`. When scripts are UpToDate but `HasActiveCompilations` queue remains undrained, honestly report `compile_active_queue_not_drained` in `checks_skipped` and clear `bIsCompiling` for gate evaluation. MCP adds `compile_await_observed_via_script_state` to `checks_performed`.

**Option 2 (not chosen for B1):** ADR-0009 `partially_completed` + `get_job_result` for compile drain would require `mcp_round_trips > 1` — document only; breaks current B1 transport bar of `mcp_round_trips == 1`.

WS-11: rerun one-call MCP fireball after orch rebuild (~`c5cd3fc`).

| Artifact | Purpose |
|---|---|
| `schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json` | Canonical full fireball envelope for MCP `CreateNiagaraEffect` |
| `extra.poc_b_gates.B1_single_request_complete` | Pipeline-complete in one `create_niagara_effect` (already on responses) |
| `extra.validation.single_request_pipeline` | Mirror of B1 on validation block |
| `poc_b_gates.never_claims` includes `mcp_transport_one_call` | Niagara **does not** self-certify MCP transport — WS-11 must |

## WS-11 harness steps

1. Load `poc_b_mcp_fireball_request.json` → serialize `request` object as `RequestJson`.
2. Single MCP `call_tool` on **`UeremcpNiagara.CreateNiagaraEffect`** (toolset registered at module startup `[VERIFIED: UeremcpNiagaraModule.cpp]`).
3. Target **`/Game/__UeremcpPoc/NS_POCB_Fireball`** (POC_ACCEPTANCE scratch root).
4. Assert response:
   - `metrics.mcp_round_trips == 1`
   - `extra.poc_b_gates.B1_single_request_complete == true`
   - `extra.validation.single_request_pipeline == true`
   - All six material roles in merged manifest (parity with `FireballInlineMaterials`)
5. Emit `scenario: poc_b_mcp_b1` evidence JSON (mirror B8 restart pattern).
6. Clean `/Game/__UeremcpPoc/` assets after run.

## Not WS-07

- **`metrics` population** — envelope layer (WS-04/WS-05)
- **MCP client wiring** — transport harness in `tests/` (WS-11)
- **Baseline / token counts** — WS-11 `docs/reviews/poc-metrics.md`

## Verification

POC-B B1 unblocks when WS-11 records one successful MCP fireball create with `mcp_round_trips == 1` and gate parity with editor PASS on the same orch tip.
