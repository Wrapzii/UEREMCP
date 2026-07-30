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

## WS-07 deliverables (landed)

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
