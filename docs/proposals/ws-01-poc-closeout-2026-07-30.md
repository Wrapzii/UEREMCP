# WS-01 POC closeout — finish residuals after A–E claims (2026-07-30)

**Owner:** WS-01 (integration finish authorization)  
**Worktree:** `UEREMCP-integration`  
**Branch:** `ws-01-poc-cde-integration`  
**Base tip before this closeout:** `109119b`  
**Final tip / local main:** fast-forwarded to this branch tip (no push). Verify with `git rev-parse main`.  
**Push:** not performed.

## Verdict

| Question | Answer |
|---|---|
| **POC-complete?** | **Yes** — POC A–E claimed under frozen `docs/POC_ACCEPTANCE.md` wording, with previously scoped E3/E4 Niagara/Material domain gates now live-gated. |
| **Production-ready?** | **No** — B10 warm-pixel / visual perfection residuals, optional D5 multi-client, Epic MCP cancel unwired, metrics cells often `unavailable`, protocol CurveFloat idempotency harness currently flaky, and broader project surfaces remain partial. |

## Finished in this closeout

1. **Niagara ADR-0006 E3/E4 + MutatingDispatch (WS-07)**
   - Repeated create/replace → `no_change_required` when emitters satisfy Spec
   - Stale `expected_revision` → `rejected` with current revision
   - `FUeremcpMutatingDispatch` on live create
2. **Material ADR-0006 E3/E4 (WS-08)**
   - Repeated create → `no_change_required` when parent + params match
   - Stale `expected_revision` → `rejected`
   - MutatingDispatch was already present
3. **Validation Domain gates (WS-11)** — NullRHI live **6/6 Success**:
   - `UEREMCP.Validation.Domain.Blueprint.*`
   - `UEREMCP.Validation.Domain.Niagara.IdempotencyRepeatedCreate`
   - `UEREMCP.Validation.Domain.Niagara.RevisionStaleRejected`
   - `UEREMCP.Validation.Domain.Material.IdempotencyRepeatedCreate`
   - `UEREMCP.Validation.Domain.Material.RevisionStaleRejected`
4. **Docs** — catalog, README, SECURITY, guide limitations, poc-metrics E rollup, residual proposals updated so C/D/E are not described as incomplete.

## Explicit residuals (not silent)

| Residual | Disposition |
|---|---|
| **D5 multi-client** | **Optional / deferred** under accepted POC D wording. No reliable unattended PIE/net harness; do not invent proof. |
| **Epic MCP `notifications/cancelled` → AICallable** | **Blocked** by Epic ToolsetRegistry adapter (no `CancelAsync` override). Use UEREMCP `cancel_job`. `[VERIFIED: ADR-0009; RB-04; UeremcpJobConstraints.cpp]` |
| **B10 warm pixels / production visibility** | Residual visual/metrics quality — does not reopen structural POC B claim text already on tip. |
| **`UEREMCP.Validation.Idempotency.RepeatedCreate` (CurveFloat harness)** | **Failing on this tip** (`created_and_validated` ×3; store replay not observed). Domain Niagara/Material/Blueprint E3 gates **PASS**. Treat protocol harness as residual flake/regression separate from domain closeout. |
| **Idempotency across editor restart** | Still session-scoped store; not claimed persistent (prior residual). |
| **Scratch / `__UeremcpPoc` cleanup** | Left in place (E1 durability evidence). Suite cleanup API: `UeremcpCleanupScratchSuite` under `/Game/__UeremcpTests/` only. Never auto-delete POC or user assets. |

## Exact commands / evidence

```powershell
cd $UEREMCP_ROOT-integration

# Build
& "$UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
  REEditor Win64 Development `
  "-Project=$UEREMCP_LEGACY_PROJECT\RE.uproject" `
  -Module=UeremcpNiagara -Module=UeremcpMaterial -Module=UeremcpValidation `
  -NoHotReloadFromIDE -WaitMutex
# Result: Succeeded

python tools/validate_schemas.py   # OK 25 schemas

# Live NullRHI domain E3/E4 (6/6 Success)
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation.Domain"
# Log: tests/integration/_logs/editor_UEREMCP_Validation_Domain_20260730_141557.log

# Also PASS: Revision.StaleRejected, Honesty.* (3)
# FAIL residual: Idempotency.RepeatedCreate protocol harness
```

## Junction

RE `Plugins/UEREMCP` was briefly retargeted to this integration worktree for build/test,
then restored to `UEREMCP-ws01` at closeout.

## Ownership note

Cross-workstream edits were authorized by the finish-UEREMCP integration task.
Commits use `[WS-nn]` subjects matching owned paths.
