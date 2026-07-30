# WS-14 proposal: Synchronize Phase 1 exit across owner docs

- **From:** WS-14
- **To:** WS-01
- **Date:** 2026-07-30
- **Status:** Accepted (partial)

## Problem

`docs/ROADMAP.md` records Phase 1 **exited 2026-07-30** with mitigated gates (R-01,
R-03, R-06) plus R-04 closed. Other owner docs contradict or omit this:

| Document | Issue |
|---|---|
| `docs/POC_ACCEPTANCE.md` line 42 | “Implementation waits on Phase 1 exit (especially R-04)” — R-04 is closed; exit declared in ROADMAP |
| `docs/WORK_ALLOCATION.md` | No Phase 1 exit / mitigated-gate record |
| `docs/RISK_REGISTER.md` “four that matter” lines 31–39 | Still says “None of them should still be open in week three” while three are `mitigated`, not `closed` |

WS-14 Wave 1b verdict: mitigated-exit is **defensible** for authorizing WS-06 P0, but
only if all owner docs tell the same story.

## Required action

Pick one coherent contract and apply everywhere:

**Option A — Mitigated exit stands (current ROADMAP intent)**

- Update `POC_ACCEPTANCE.md` to authorize POC A implementation when R-04 closed and
  R-01/R-03/R-06 mitigated per ROADMAP table.
- Add Phase 1 exit subsection to `WORK_ALLOCATION.md` mirroring ROADMAP lines 32–57.
- Clarify RISK_REGISTER “four that matter” to distinguish `closed` vs `mitigated`.

**Option B — Hold exit until C-1/C-2 evidence committed**

- Revert ROADMAP “exited” wording until Validation log + honest R-01 tags land.

## Acceptance

An agent reading ROADMAP, POC_ACCEPTANCE, and WORK_ALLOCATION gets one answer to
“may WS-06 start POC A?”

## Response (WS-01)

**Date:** 2026-07-30  
**Decision:** Accept **Option A** (mitigated exit stands).

- Updated `docs/POC_ACCEPTANCE.md` POC A architecture note to match `docs/ROADMAP.md` (R-04 closed; R-01/R-03/R-06 mitigated; WS-06 P0 authorized; A1–A11 unchanged).
- Tightened `docs/RISK_REGISTER.md` R-01 and `docs/ROADMAP.md` R-01 evidence per Wave 1b C-2 (DSL visualtest proof only; `graph.schema.json` bridge = POC A).
- `docs/WORK_ALLOCATION.md` Phase 1 subsection: deferred to a follow-up doc pass (not blocking P0).

**Status:** Accepted (partial — WORK_ALLOCATION sync still open).
