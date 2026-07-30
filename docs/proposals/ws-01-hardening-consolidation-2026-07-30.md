# WS-01 production-hardening consolidation (2026-07-30)

**Owner:** WS-01  
**Worktree:** `UEREMCP-hardening-integration`  
**Branch:** `ws-01-hardening-integration`  
**Base `main`:** `227cb9914b1fceec66498f4f74293177ad4b225b`  
**Push:** not performed.

## Sources landed

| Branch | Tip SHA | Merge strategy | Outcome |
|---|---|---|---|
| `ws-04-transport-cancel-hardening` | `ded9fc4` | Fast-forward then merge history | Cooperative `cancel_job` editor-verified; Epic `notifications/cancelled` documented as immutable UE 5.8 ToolsetRegistry adapter limitation |
| `ws-05-idempotency-persistence-hardening` | `9cc5381` (`2f3902a` + `9cc5381`) | Merge | Durable Claim/Complete under `Saved/UEREMCP/idempotency`; CurveFloat RepeatedCreate flake fixed; restart Create/Verify pair |
| `ws-11-multiplayer-visual-hardening` | `deacfa6` | Merge | D5 multi-client live PASS; B10 `VisibleRender` warm-pixel reconfirm PASS |

All three tips were ancestors of / based on `227cb99`. Merge conflicts: none (`tests/README.md` auto-merged cleanly).

## What closed

| Item | Evidence |
|---|---|
| **D5 genuine multi-client** | `UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE`; artifact `tests/integration/_artifacts/d5_pattern_b_multiclient.json` (`status=pass`, 2 remote clients) |
| **B10 rendered warm-pixel gate** | `UEREMCP.Niagara.POCB.VisibleRender`; PNG supplementary only |
| **Cooperative `cancel_job`** | `UEREMCP.Transport.JobRegistry.Cancel` drives production scheduler; handoff `ueremcp_cancel_job_action: true` |
| **Durable idempotency + CurveFloat flake** | Protocol Claim/Complete; isolated memory store for RepeatedCreate; restart pair via `tests/run_idempotency_restart.ps1` |

## WS-01 wording landed this consolidation

- `docs/POC_ACCEPTANCE.md` — D5 live multi-client harness; B10 programmatic warm-pixel criterion
- `docs/CAPABILITY_CATALOG.md` — `get_job_result` / `cancel_job` available with Epic cancel limitation; `execute_plan` durable Claim/Complete + honest caveats; `create_spell` / `create_niagara_effect` closed-proof notes
- `README.md` — residuals list updated
- `docs/proposals/ws-01-poc-closeout-2026-07-30.md` — residual table supersession column

## Remaining production limitations (honest)

1. **Epic MCP `notifications/cancelled`** does **not** cancel ToolsetRegistry/AICallable work on UE 5.8 — adapter has no `CancelAsync` override. Agents must use UEREMCP `cancel_job(job_id)`. `[VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h; ws-04-cancellation-hardening-closeout.md]`
2. **Durable idempotency** — metadata + package files are not one atomic transaction; crash-after-mutation-before-completion leaves an in-progress claim (~1h reclaim); legacy `Put`/`TryGetReplay` call sites lack fingerprint conflict detection until migrated (`execute_plan` is migrated).
3. **Metrics** — many WS-14 cells remain `unavailable`; overall POC-B metrics close is not claimed.
4. **Production visual perfection** — B10 gate PASS does not claim fireball looks correct on every scene/hardware/quality setting.
5. **WS-12 / WS-13 adoption pending** — SECURITY.md and `docs/guide/**` still carry pre-hardening cancel SKIP residual wording; owners should adopt `ws-04-cancellation-hardening-closeout.md` (WS-01 did not edit those paths).

## Verification (this consolidation)

| Check | Result |
|---|---|
| `python tools/validate_schemas.py` | **OK** — 25 schemas |
| `python tools/check_ownership.py --ws WS-01` | **OK** — WS-01 wording paths only |
| `python tests/run_unit_tests.py` | **OK** — 48 tests |
| `python tests/unit/test_d5_multiclient_harness.py` | **OK** — 2 tests |
| `python Plugins/.../scripts/test_transport_constraints.py` | **OK** — cancel_job active; Epic notification limitation closed |

Editor NullRHI / multi-client / Transport C++ suites were **not** re-run here to avoid
fighting other worktrees for the RE `Plugins/UEREMCP` junction. Runtime evidence
remains on the source branch closeouts (WS-04/WS-05/WS-11).

## Local main

Final tip: `33aff30` (WS-01 wording) atop merges of `ded9fc4` / `9cc5381` /
`deacfa6`. Fast-forward `main` to that tip. Verify with `git rev-parse main`.
Do not push.
