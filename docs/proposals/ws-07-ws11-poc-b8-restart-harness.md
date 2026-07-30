# WS-07 → WS-11: POC B B8 editor-restart harness

**Status:** Open  
**Owner:** WS-11 (`tests/**`, `UeremcpValidation/**`)  
**WS-07 deliverable:** `B8_assets_saved` gate only (disk save verified in-process)

## What WS-07 surfaces now

`extra.poc_b_gates` on mutating `create_niagara_effect`:

| Field | Meaning |
|---|---|
| `B8_assets_saved` | `options.save=true`, `niagara.save_package` performed, `validation.saved=true` |
| `B8_restart_survival` | always **null** — never inferred |

`never_claims` includes `editor_restart_survival`.

This satisfies the **save** half of POC B criterion B8 honestly. It does **not** claim assets survive editor restart (POC B B8 / POC E1).

## WS-11 ask

Extend `tests/run_poc_acceptance.ps1` / `poc_evidence.py` fireball path (or add sibling runner) to:

1. Run full fireball create under `/Game/__UeremcpPoc/` (materials + six emitters).
2. Emit `scenario: poc_b8_create` evidence with checkpoint ID + asset list (system + MIs).
3. Restart editor (existing pattern from POC E1 tooling).
4. Emit `scenario: poc_b8_verify` reloading the same assets from registry.
5. Assert `B8_restart_survival=true` only after verify — not from create response alone.

Reference: `docs/proposals/ws-11-poc-evidence-handoff.md`, `tests/poc_evidence.py`.

## Related: extend FireballInlineMaterials filter

Current filter (`UEREMCP.Niagara.POCB.FireballInlineMaterials`) asserts **B2/B4 only**. After this WS-07 gate land, extend it (or add `POCB.FireballAcceptanceGates`) to assert from one create response:

- `B1_single_request_complete`
- `B3_six_emitters_present`
- `B5_user_parameters_present`
- `B6_compile_awaited`
- `B9_change_manifest_complete`
- `validation.single_request_pipeline`

Still **not** overall POC-B until B8 restart + MCP transport proof (B1 MCP) are green.
