# WS-14 proposal: Commit shipping Validation automation log

- **From:** WS-14
- **To:** WS-11
- **Date:** 2026-07-30
- **Status:** Open

## Problem

R-03 mitigation and ADR-0005 shipping Verification cite
`tests/integration/_logs/editor_UEREMCP_Validation_20260729_234458.log`
(`[VERIFIED-RUNTIME]`). That file is **not in the repository**. WS-14 cannot
independently confirm `UEREMCP.Validation.Rollback.MultiAssetDiscard` green on the
shipping plugin graph.

Contrast: `editor_UeremcpCore_ReferenceToolset_20260730_001348.log` **is** committed and
shows `Result={Success}` for Core tests.

## Required action

1. Re-run on RE:

   ```powershell
   pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"
   ```

2. Commit redacted log under `tests/integration/_logs/` (follow `_logs/README.md`).

3. Update `Rollback.MultiAssetDiscard.md` status table if the filename changes.

## Acceptance

Critic can grep the committed log for
`UEREMCP.Validation.Rollback.MultiAssetDiscard` and `Result={Success}` without
re-running the editor.
