# WS-07 → WS-11: POC B B8 editor-restart harness

**Status:** Create/Verify filters landed on WS-07 (`UeremcpNiagaraPocBRestartTests.cpp`)  
**Owner:** WS-11 orchestrates two-process `run_poc_acceptance.ps1 -Scenario B8`

## WS-07 filters (landed)

| Filter | Role |
|---|---|
| `UEREMCP.Niagara.POCB.Restart.Create` | Full fireball create under `/Game/__UeremcpPoc/`, writes `Saved/UEREMCP/poc_b8_restart_checkpoint.json`, emits `poc_b8_create` evidence |
| `UEREMCP.Niagara.POCB.Restart.Verify` | Fresh editor process: reloads checkpoint assets from registry, emits `poc_b8_verify` with `B8.status=pass`, cleans POC assets |

Fixture handoff: `schemas/domains/niagara/fixtures/poc_b8_restart_handoff.json`

Create embeds the fireball request inline when `-UeremcpPocBScaffold` is omitted (RE plugin junction has no repo schemas). Optional fixture args still supported.

`poc_b_gates.B8_restart_survival` remains **null** on create responses — restart proof lives only in verify evidence.

## WS-11 orchestration (still required)

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

## Related: FireballInlineMaterials B3/B5/B6/B9

Owned by WS-11 (`UeremcpValidation/.../NiagaraPocBFireballMaterials.spec.cpp`). WS-07 surfaces the gate fields on `poc_b_gates`; extending filter assertions is WS-11 follow-up.
