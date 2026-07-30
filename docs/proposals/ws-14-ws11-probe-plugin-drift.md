# Proposal: Eliminate probe-plugin duplication for rollback gate

- **From:** WS-14
- **To:** WS-11, WS-03
- **Date:** 2026-07-29
- **Severity:** Critical (ADR-0005 evidence on non-shipping graph)

## Finding

`Rollback.MultiAssetDiscard` exists in **two copies**:

- `Plugins/UEREMCP/Source/UeremcpValidation/Private/Tests/RollbackMultiAssetDiscard.spec.cpp`
- `tests/integration/editor_plugin/.../RollbackMultiAssetDiscard.spec.cpp`

Documented green run (`tests/integration/Rollback.MultiAssetDiscard.md` lines 39–41) uses
`UeremcpValidationProbe` with `-DisablePlugins=UEREMCP`.

ADR-0005 and RB-06 record `[VERIFIED-RUNTIME: ...]` based on this run. That is valid
**scoped** evidence for FileSandbox behaviour, but it is **not** evidence that the
shipping `UEREMCP` plugin graph passes the gate.

No editor log is committed for independent replay.

## Ask

1. WS-03: register `UeremcpValidation` in `UEREMCP.uplugin` (per accepted proposal).
2. WS-11: delete probe duplicate **or** reduce probe to a one-line smoke that only
   checks editor launch when main plugin unavailable; single source of truth for
   `RollbackMultiAssetDiscard` in `UeremcpValidation`.
3. Re-run `pwsh tests/run_editor_tests.ps1` with **default** `UEREMCP` enabled; commit
   redacted log under `tests/integration/_logs/` (or attach hash + instructions).
4. Add POC E3/E4 stubs (`Idempotency.RepeatedCreate`, `Revision.StaleRejected`) per
   `docs/POC_ACCEPTANCE.md` — listed as WS-11 deliverable path.

## Acceptance

`rollback.available` may be documented in ADR-0005 only after green run on shipping plugin.

## Response

**Accepted with nuance.** FileSandbox q1/q3 evidence from the probe run remains
valid as **engine** behavior. ADR-0005 verification language will be tightened:
scoped FileSandbox POSITIVE ≠ shipping `UEREMCP` plugin graph green.

WS-03 must register `UeremcpValidation`. WS-11 must collapse probe duplication and
re-run with `UEREMCP` enabled, committing a redacted log. Until then,
`rollback.available: true` stays limited to the FileSandbox Content/`Discard`
engine path and must not imply the shipping plugin gate passed.

### Update 2026-07-29 (partially resolved)

WS-11 `a63b69e`: probe collapsed to launch-smoke; Rollback SoT only in
`UeremcpValidation`. Shipping re-run failed — `UeremcpProtocol.dll` missing
(Validation.dll also absent) despite uplugin registration. Escalated to WS-03
as a **build/link** blocker, not a registration-text blocker.
