# WS-01 integration consolidation — 2026-07-30 (phase 1)

- **Owner:** WS-01
- **Mandate:** Produce one clean validated integration tip. Do **not** start POC C/D/E.
- **Integration worktree:** `$UEREMCP_ROOT-integration`
- **Branch:** `ws-01-poc-cde-integration`
- **Base tip:** `ws-07-niagara-status-honesty` @ `b84397fa6ccbe92fe45fd2cdf7b9efd2b6f8aac7`
- **Final tip:** `ab711c2aa045cb582a53b700b6953705cf0059dc` (parent `b84397f`)

## What “merge everything” meant here

Consolidate **validated, non-duplicate** work already on the Niagara status-honesty
lineage. Do **not** merge every historical side branch. Preserve separate workstream
commits; do not squash.

Audit method: exact ancestor checks, `git cherry` / patch-id equivalence, content
presence for known deliverables. Side-branch unique SHAs that are superseded
rewrites of already-integrated content were **not** re-cherry-picked (would overwrite
newer tip content with older patches).

## Validated commits — already present (exact SHA ancestors of tip)

| Label | SHA | Subject |
|---|---|---|
| POC A claim lineage | `3756244` | `[WS-01] Record Python-free A5 rerun readiness` |
| Overall POC B claim | `69aeac8` | `[WS-01] Claim overall POC B` |
| WS-11 claimed POC-B bundle | `0038016` | `[WS-11] Refresh claimed POC B evidence bundle` |
| Niagara validated status | `e32d866` | `[WS-07] Return validated Niagara create status` |
| Live status record | `d3e0f95` | `[WS-01] Record validated Niagara create status` |
| Handoff close | `b84397f` | `[WS-01] Close validated-status and bundle handoff rows` |
| WS-04 timeout job scheduler | `dae0e5c` | `[WS-04] Wire timeout job scheduler` |
| WS-12 security adoption | `bde401d` | `[WS-12] Harden domain security adoption surface` |
| WS-08 Material MutatingDispatch | `d3e35cd` | `[WS-08] Gate Material mutators through security dispatch` |
| WS-05 PlanActions / execute_plan | `bd9b2ba` | `[WS-05] Expose agent-facing execute_plan adapter` |
| WS-03 AICallable ExecutePlan | `fc98fbc` | `[WS-03] Expose execute_plan through reference toolset` |
| WS-05 understood.action fix | `656916b` | `[WS-05] Fix execute-plan understood action regression` |
| WS-06 Blueprint patch disposition | `6decd88` | `[WS-06] Reject undefined Blueprint patch semantics` |
| WS-13 guides | `74590a7` | `[WS-13] Complete agent usage and limitations guides` |
| WS-14 metrics harness + baseline | `2aab525` | `[WS-14] Archive successful POC-B primitive metrics` |
| WS-07 primitive fixture fixes | `24fbe95` | `[WS-07] Fix primitive mesh binding wire key` |

## Equivalent content already present (different SHA; do not re-cherry-pick)

| Side-branch SHA | Tip equivalent | Notes |
|---|---|---|
| `4a320e8` Templates post-engine-init | `45fd0ef` | Same subject; different patch-id |
| `ede54a1` acceptance-gap audit | `c4edd99` | Same subject; already on tip |
| `528f16e` warm material B10 pass | `6cc1b7a` | Same subject; already on tip |
| `f8b9663` B10 screenshot evidence | `87d6c81` | Same subject; tip has production PNG |
| `d7ee2a6` Niagara post-engine-init | evolved on tip | `UeremcpNiagaraModule` uses `FCoreDelegates` post-engine-init |
| `f2cbf5b` B10 viewport compile fix | superseded | Tip’s `NiagaraPocBVisibleRender.spec.cpp` is newer/larger; cherry would regress evidence |

## Newly included in this consolidation commit

| Item | Action |
|---|---|
| `tests/integration/_logs/poc_a_complete_round_trip_3756244.json` | Force-added. File existed only as **gitignored local dirt** under ws01 (`tests/.gitignore` → `integration/_logs/*`). Cited by the overall POC A claim; A1–A11 all `pass`, `outcome=pass`. Same force-add pattern as committed POC B claim logs. |
| This proposal | WS-01 integration record |

**No cherry-picks of side-branch commits were required.** Tip already contained all listed validated SHAs / equivalents.

## Deliberately excluded

### Uncommitted ws01 dirt (preserved in ws01; not on integration)

- `docs/proposals/ws-02-reagenttools-schema-dumps.md` (modified)
- `docs/proposals/ws-03-protocol-ue58-json-keys.md` (modified)
- `docs/proposals/ws-08-niagara-material-export-handoff.md` (modified)
- `schemas/domains/materials/test_flipbook_import_scaffold.py` (modified)
- `docs/proposals/ws-08-ws05-execute-plan-material-handlers.md` (untracked)

ws01 remained on `ws-07-niagara-status-honesty` @ `b84397f` with that dirt untouched.
Integration worktree was created without switching/stashing/resetting ws01. RE plugin
junction was **not** retargeted.

### Side branches with unique SHAs but superseded / non-validated / POC-CDE scope

| Branch | Why excluded from further cherry-pick |
|---|---|
| `ws-03-plugin`, `ws-04-transport`, `ws-05-protocol`, `ws-06-blueprint`, `ws-08-material` | Unique early SHAs; tip has newer integrated equivalents (`git cherry` `+` are obsolete rewrites) |
| `ws-07-niagara`, `ws-07-*` experimental/hotfix branches | Superseded by status-honesty lineage; remaining `+` patches are obsolete compile-await experiments or duplicate registration |
| `ws-14-poc-b-metrics` / `ws-14-review` residual critic commits | Failed post-fix trials / stale critic refreshes; successful baseline already on tip via `2aab525` |
| `ws-11-editor-gate-runtime-followup` unique `f2cbf5b` | Superseded; tip already has B10 visible-render evidence |
| `ws-09-gameplay` unique SHAs | Content already equivalent on tip by patch-id; POC D work is **next phase**, not additional cherry-picks here |
| `ws-01-acceptance-gap-audit-2026-07-30` | Equivalent `c4edd99` already on tip |

## Ownership note (`check_ownership.py --ws WS-01`)

`python tools/check_ownership.py --ws WS-01 --base main` reports hundreds of
non-WS-01 paths. That is **expected and not meaningful** on a multi-workstream
integration branch: the tool assumes a single-WS diff against base. Aggregate
ownership is instead recorded via commit subject tags since `main` (320 commits):

| Tag | Count |
|---|---:|
| WS-01 | 99 |
| WS-07 | 59 |
| WS-08 | 41 |
| WS-11 | 25 |
| WS-06 | 19 |
| WS-10 | 15 |
| WS-15 | 13 |
| WS-05 | 10 |
| WS-03 / WS-09 | 9 each |
| WS-04 / WS-14 | 8 each |
| WS-12 | 2 |
| WS-13 | 1 |
| OTHER | 2 |

Integration hygiene for this phase: **working tree clean** (no uncommitted dirt on
the integration branch), not single-WS ownership of the whole history.

## Validation results (offline; RE junction not retargeted)

| Check | Result |
|---|---|
| `python tools/validate_schemas.py` | OK — 24 schemas |
| `python tests/run_unit_tests.py` | OK — 39 tests |
| `python docs/guide/check_guide_links.py` | OK |
| `python -m unittest docs.reviews.metrics.test_metrics_harness` | OK |
| `docs/reviews/metrics/acceptance.py` | exit 0 |
| Security / transport / material security contracts | OK |
| Protocol / Blueprint / Templates / Animation Python suites | OK |
| Material / Niagara / Gameplay domain schema unit suites | OK |
| `git status --porcelain` | clean (after this commit) |
| Tracked `Binaries/` `Intermediate/` `Saved/` `__pycache__/` | none |

### Evidence parse

| Artifact | Result |
|---|---|
| `tests/integration/_logs/poc_a_complete_round_trip_3756244.json` | present after force-add; `outcome=pass`; A1–A11 all `pass`; metrics 3 MCP / 4 internal / 2.30s |
| `tests/integration/_logs/poc_b_current_lineage_2aab525.json` | `overall_poc_b_claimed=true` |
| `docs/reviews/metrics/artifacts/poc_b_primitive_baseline_fixed_20260730.json` | `status=created_and_validated`; `primitive_ops_executed_per_trial=63` |
| `tests/integration/_artifacts/poc_b10_fireball.png` | present (93837 bytes) |
| `tests/integration/_artifacts/poc_b10_canary.png` | present (31052 bytes) |

## Blockers before POC C / D / E

1. **Editor rebuild against RE not run in this phase** (by mandate: do not retarget the
   RE plugin junction away from ws01). Offline suites are green; live editor
   regression is required before claiming C/D/E criteria.
2. **POC C/D/E themselves are not claimed** and were not started.
3. No missing validated *code* commits were found. The only tip gap closed here was
   the untracked POC A CRT JSON cited by the existing claim docs.

## Next base for POC C, D, and E

Use this integration branch tip (post-consolidation commit) as the **single shared
base**:

```text
worktree: $UEREMCP_ROOT-integration
branch:   ws-01-poc-cde-integration
base SHA: b711c2aa045cb582a53b700b6953705cf0059dc (or later ws-01-poc-cde-integration HEAD)
```

Recommended launch (phase 2 — **not done here**):

1. Keep ws01 / RE junction undisturbed until an explicit cutover.
2. Junction RE’s UEREMCP plugin to `UEREMCP-integration` only when ready for editor
   proof, or create per-POC worktrees branched from this tip.
3. Suggested editor regression command (exact; run after safe junction):

```powershell
# From the RE project host, after plugin path points at UEREMCP-integration:
powershell -File tests/run_editor_handoff_gates.ps1
powershell -File tests/run_poc_acceptance.ps1
```

4. Workstream split from this tip:
   - **POC C** — follow `docs/POC_ACCEPTANCE.md` POC C owner/criteria (templates /
     multi-asset composition as defined there)
   - **POC D** — WS-09 from this tip (`UeremcpGameplay` already registered)
   - **POC E** — metrics/cross-domain acceptance per POC_ACCEPTANCE / WS-14

Do **not** merge to `main`/`master`, push, or open a PR from this phase-1 tip until
WS-01 explicitly requests it.
