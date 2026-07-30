# Proposal: idempotency store location (ADR-0006)

- **From:** WS-05
- **To:** WS-01, WS-03, WS-12
- **Date:** 2026-07-29
- **Addresses:** ADR-0006 open question — where the idempotency store lives

## Decision for v1

**In-memory, process-scoped, editor-session lifetime.**

Implemented as `FUeremcpIdempotencyStore` in `UeremcpProtocol`. A repeat of the
same `idempotency_key` returns the stored response JSON; callers set
`metrics.replayed: true` and perform no work.

## Why not durable yet

- Surviving editor restart is preferred by ADR-0006 but not the minimum bar.
- Durable store location (project `Saved/`, sandbox, or user settings) couples to
  WS-12 permission tiers and WS-03 plugin lifecycle — not owned by WS-05 alone.
- Empty keys are ignored (no accidental global singleton entry).

## Follow-up

When WS-03 confirms plugin module lifetime and WS-12 confirms an allowed
writable root, WS-05 can add an optional disk-backed store behind the same
interface without changing the envelope.
## Response

**Accepted for v1.** In-memory, process-scoped, editor-session lifetime meets
ADR-0006's minimum bar. Durable store deferred until WS-03 lifecycle + WS-12
writable-root answers exist. ADR-0006 open question updated accordingly.
