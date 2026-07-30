# Proposal: idempotency store location (ADR-0006)

- **From:** WS-05
- **To:** WS-01, WS-03, WS-12
- **Date:** 2026-07-29
- **Updated:** 2026-07-30
- **Addresses:** ADR-0006 open question — where the idempotency store lives

## Decision for v1

**In-memory, process-scoped, editor-session lifetime** as the minimum bar.

Implemented as `FUeremcpIdempotencyStore` in `UeremcpProtocol`. A repeat of the
same `idempotency_key` returns the stored response JSON via `TryGetReplay` with
`metrics.replayed: true` and performs no work.

## Wave 2 durability (landed)

Disk persistence is now implemented behind the same API:

```
<ProjectSavedDir>/UEREMCP/idempotency/<sha256(utf8(key))>.json
```

- Accepted root from `docs/proposals/ws-12-idempotency-store-root.md` / ADR-0010 §6.
- Never under `Intermediate/Sandboxes/`.
- Default process store (`Get()`) enables durable Put + hydrate-on-miss.
- `Clear()` is memory-only so domain test harnesses do not wipe project Saved data
  unless they intentionally call `PurgeDurable()` on an overridden temp root.
- See `Plugins/UEREMCP/Source/UeremcpProtocol/Docs/IDEMPOTENCY.md`.

## Why the root was deferred originally

- Surviving editor restart is preferred by ADR-0006 but was not the Wave 1 minimum.
- Durable location needed WS-12 path policy confirmation (now accepted).

## Response (historical)

**Accepted for v1.** In-memory, process-scoped, editor-session lifetime meets
ADR-0006's minimum bar. Durable store deferred until WS-03 lifecycle + WS-12
writable-root answers exist. ADR-0006 open question updated accordingly.

**2026-07-30:** WS-05 landed durable persistence under the accepted Saved root
without changing the envelope or requiring Gameplay call-site changes.
