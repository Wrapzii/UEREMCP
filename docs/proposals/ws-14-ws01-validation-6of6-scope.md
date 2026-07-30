# WS-14 proposal: Narrow Validation 6/6 / ADR-0006 scope wording

- **From:** WS-14
- **To:** WS-01 (RISK_REGISTER, ADR-0006 runtime note), WS-11 (test summaries)
- **Date:** 2026-07-30
- **Blocks:** Honest POC E3/E4 and domain idempotency claims
- **Review:** `docs/reviews/wave-2-2026-07-30.md` H-2

## Problem

“Validation 6/6” is being used as shorthand for ADR-0006 + R-03 readiness. The suite mixes:

| Tests | What they prove |
|---|---|
| Rollback.* (3) | FileSandbox discard scopes (R-03 / ADR-0005) — real editor packages |
| Idempotency.RepeatedCreate | In-memory store + `metrics.replayed` on **scratch CurveFloat harness** |
| Revision.StaleRejected | Protocol revision guard on **scratch harness** — “full graph pipeline not wired” |

Harness source (`IdempotencyRepeatedCreate.spec.cpp:61–64`) and SUCCESS-log AddInfo both
say protocol harness, not domain pipeline. Repeat statuses remain
`created_and_validated` (stored replay), which is ADR-legal but easy to misread as
triple create if `metrics.replayed` is omitted from summaries.

## Ask

1. ADR-0006 “Runtime status” and RISK_REGISTER: say **protocol + scratch harness**, not
   “ADR-0006 verified for domains.”
2. Keep POC E3/E4 open until a Blueprint/Niagara (or shared domain) path uses the store.
3. Prefer table language “6/6 Validation automation (3 rollback + 2 protocol harness + smoke)”
   over bare “Validation 6/6.”
