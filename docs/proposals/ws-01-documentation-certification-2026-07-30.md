# WS-01 documentation certification (2026-07-30)

**Owner:** WS-01  
**Worktree:** `UEREMCP-doc-certification`  
**Branch:** `ws-01-doc-certification`  
**Implementation parent (tested tip before this certification):** `6a611cf70e598a4dd99fbab2eed7eb62c8274233`  
**Push:** not performed.

## Purpose

Implementation-quality audit of authoritative / current documentation after POC A–E
closeout and production-hardening consolidation. Distinguishes immutable historical
records from current truth. Does not rewrite historical evidence; adds supersession
notes where current docs would otherwise contradict older tips.

## Certification SHA strategy (avoid self-reference)

1. Documentation edits and this artifact land on `ws-01-doc-certification` based on
   parent **`6a611cf`**.
2. Verification commands in this file were run against the working tree that includes
   those edits (before or at commit time).
3. **Final local `main` SHA** is recorded only after the certification commits land and
   `main` is ancestor-safe fast-forwarded. That tip SHA is the certification tip; it is
   **not** embedded inside this file as a circular pin. Operators verify with:

```powershell
git rev-parse main
git merge-base --is-ancestor 6a611cf main
```

## Scope — authoritative / current

| Document | Owner | Role |
|---|---|---|
| `AGENTS.md` | WS-01 | Operating contract |
| `README.md` | WS-01 | Human entry + POC status |
| `docs/WHY.md` | WS-01 | Cost model |
| `docs/GROUNDED_FACTS.md` | WS-01 | Verified UE 5.8 surface |
| `docs/ROADMAP.md` | WS-01 | Phase status |
| `docs/RISK_REGISTER.md` | WS-01 | Risk statuses |
| `docs/CAPABILITY_CATALOG.md` | WS-01 | Action registry |
| `docs/POC_ACCEPTANCE.md` | WS-01 | Binary POC gates |
| `docs/WORK_ALLOCATION.md` | WS-01 | Ownership |
| `docs/SECURITY.md` | WS-12 | Security / cancel / mutator gates |
| `docs/guide/**` | WS-13 | Agent guides |
| `docs/adr/**` | WS-01 | Frozen ADRs (not rewritten) |
| `docs/audit/README.md` + indexes | WS-02 | Audit indexes (read-only here) |
| `schemas/README.md` + contracts | WS-01 / domain WS | Schema docs |
| Closeouts | various | Evidence — historical + current |

## Historical records (do not falsify)

These remain historical evidence. Current truth is the catalog / RISK_REGISTER /
SECURITY / guides / closeout supersession notes — not a rewrite of these files.

| Record | Why historical |
|---|---|
| `docs/proposals/ws-14-ws12-security-not-wired.md` | Predates MutatingDispatch adoption and MutatorQueue implementation |
| `docs/reviews/wave-2-*.md` | Point-in-time critic snapshots |
| Residual rows in `ws-01-poc-closeout-2026-07-30.md` | Superseded by hardening consolidation column |
| Hardening item 5 "WS-12/13 adoption pending" | Superseded by `f2513a7` / `6a611cf` (noted in hardening proposal) |
| Older tip SHAs in research / metrics fixtures | Evidence of the run that produced them |

## Contradictions fixed in this certification

| Issue | Fix |
|---|---|
| `RISK_REGISTER` R-07/R-12 still "open" with "implement Security/tests/queue" | Updated to **mitigated** with code-backed residuals (ungated paths; not universal close) |
| `ROADMAP` Phases 2–5 read as not-started | Added POC claim banner + per-phase status reflecting A–E / hardening |
| Root `README` "plugin scaffold (not yet compiled)" | Corrected to shipping plugin on RE |
| Catalog `instantiate_template` "POC C not started" | Corrected — POC C claimed |
| Catalog intro implied POC-B visibility still incomplete | Narrowed to metrics / perfection / Animation residuals |
| `SECURITY` "R-07 closed" for only Niagara/Material vs incomplete residual | **Mitigated** for wired mutators + **residual remains** for ungated paths; R-12 mitigated |
| Security contract test required stale "R-07 remains open until domain adoption" | Updated contract to require mitigated + residual wording |
| Guides: B10 residual / "no overall POC-B" / D5 static-only / tip `164a300`/`dae0e5c` | Aligned with B10 PASS, D5 live, parent `6a611cf` |
| Guide claimed `ExecutePlan` not AICallable | Corrected — `UUeremcpReferenceToolset::ExecutePlan` is AICallable |
| Hardening consolidation still said WS-12/13 adoption pending | Supersession note pointing at `f2513a7` / `6a611cf` |

## Final-state claims (evidence-backed)

| Claim | Verdict |
|---|---|
| POC A–E complete (accepted criteria) | **Yes** — claimed |
| D5 multi-client | **Closed live** |
| B10 rendered warm-pixel | **Closed live** |
| Niagara/Material Domain E3/E4 | **Closed live** |
| Durable idempotency (`execute_plan`) | **Closed** with crash/migration caveats |
| Cooperative `cancel_job` | **Available / editor-verified** |
| Epic `notifications/cancelled` → AICallable | **Immutable UE 5.8 adapter limitation** — not fixable in UEREMCP without Engine patch / ADR-0002 violation |
| Production-ready | **No** |
| R-07 / R-12 | **Mitigated** for gated live mutators; residuals documented |

## Checks run

| Check | Result |
|---|---|
| `python tools/validate_schemas.py` | OK — 25 schemas |
| `python docs/guide/check_guide_links.py` | OK — relative links + fixture citations |
| `python tools/check_ownership.py --ws WS-01` | OK for WS-01 paths (run per commit) |
| `python tools/check_ownership.py --ws WS-12` | OK for Security paths |
| `python tools/check_ownership.py --ws WS-13` | OK for guide paths |
| `python Plugins/.../scripts/test_security_contract.py` | OK after contract update |
| `python Plugins/.../scripts/test_transport_constraints.py` | OK — cancel_job active; Epic limitation closed |
| `python tests/run_unit_tests.py` | OK — 48 tests |
| `python docs/reviews/metrics/test_metrics_harness.py` | OK |
| Encoding / mojibake scan on authoritative docs | No mojibake / BOM hits |
| Stale-status search (`implement queue`, `Wave 2 UeremcpSecurity`, `not yet compiled`, `POC C not started`, guide B10 residual) | Cleared in current docs; historical proposals left intact |

Editor NullRHI / multi-client / Transport C++ suites were **not** re-run in this
worktree (RE `Plugins/UEREMCP` junction contention). Runtime evidence remains on the
hardening / domain closeouts already merged at `6a611cf`.

## Exact remaining limitations

1. Epic MCP `notifications/cancelled` cannot cancel ToolsetRegistry/AICallable work on
   UE 5.8 (`CancelAsync` not overridden on private adapter). Use `cancel_job(job_id)`.
2. Durable idempotency: non-atomic metadata+package; ~1h reclaim after crash mid-flight;
   legacy `Put`/`TryGetReplay` sites lack fingerprint conflict detection.
3. Metrics cells often `unavailable`; E7 / R-17 overall close not claimed.
4. B10 PASS ≠ production visual perfection across scenes/hardware/quality.
5. R-07/R-12 residuals: ungated mutate paths (Animation writes if added; Templates
   promote; future domains); tag/INI concurrency outside the mutator queue.
6. Experimental engine APIs (R-02); FileSandbox scopes outside proven rollback (R-03
   residual); Animation authoring unsupported; discovery actions still planned.
7. Not production-ready for unattended multi-agent fleets without operator loopback
   discipline and UnrealWatch companion.

## Commits intended on this branch

1. `[WS-01] …` — RISK_REGISTER, ROADMAP, README, catalog, POC_ACCEPTANCE, hardening
   supersession, this certification artifact
2. `[WS-12] …` — SECURITY.md + security contract test
3. `[WS-13] …` — `docs/guide/**`

Then fast-forward local `main` to the branch tip (ancestor-safe from `6a611cf`).
Do not push.
