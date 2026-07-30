# WS-07 → WS-11: POC B B10 visible render (supplementary)

**Status:** Open — not implemented in WS-07 Niagara  
**Owner:** WS-11 harness; viewport capture outside Niagara domain

## Requirement (POC_ACCEPTANCE.md B10)

Fireball must **visibly render** when placed. Screenshot is **supplementary evidence only** — never substitutes for B4/B7 programmatic validation.

## WS-07 surface (honest null)

`extra.poc_b_gates.B10_visible_render` is **`null`** on all `create_niagara_effect` responses.  
`checks_skipped` includes `niagara.poc_b.B10_visible_render`.

`inspect_system` exposes renderer topology and material paths (`inspect_fidelity`) but **does not** capture viewport pixels or run PIE smoke.

`extra.validation.runtime_smoke_test` remains **`null`** (unchanged).

## Proposed WS-11 path (no WS-07 code required for minimal B10)

1. After MCP/editor fireball create, spawn `NS_POCB_Fireball` in a scratch level under `/Game/__UeremcpPoc/`.
2. Capture viewport screenshot via editor automation or Epic automation plugin (WS-11 owned).
3. Attach image path/hash to `poc_b_mcp_b1` or separate `poc_b10_render` evidence record.
4. Do **not** assert `created_and_validated` from screenshot alone.

## If programmatic render readiness is needed later

Proposal to WS-03/WS-11 (not WS-07):

- Optional `options.runtime_smoke: true` on envelope → dispatches to editor viewport/PIE harness
- Or dedicated `UEREMCP.Validation.POCB.RenderSmoke` automation filter

WS-07 will not add screenshot tools to `UeremcpNiagara` without ADR/proposal acceptance.
