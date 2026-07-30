# Idempotency store (ADR-0006)

**Owner:** WS-05  
**Authority:** `docs/adr/ADR-0006-idempotency-revisions.md`,
`docs/proposals/ws-12-idempotency-store-root.md`

## Behaviour

`FUeremcpIdempotencyStore` records `(idempotency_key → response JSON)` on verified
completion. A later request with the same key returns that response via
`TryGetReplay`, which sets `metrics.replayed: true` and the current `request_id`,
and performs no domain work.

Empty keys are ignored.

## Durability

Wave 1 in-memory caching remains. Wave 2 disk durability is enabled by default on
each store instance:

```
<ProjectSavedDir>/UEREMCP/idempotency/<sha256(key)>.json
```

Never under `Intermediate/Sandboxes/` — sandbox Discard must not erase retry
records `[VERIFIED: docs/proposals/ws-12-idempotency-store-root.md]`.

Each durable file is a versioned record containing the original key and the
response object. Lookup verifies the stored key matches before hydrating the
session cache, so a hash collision cannot silently replay the wrong response.

`Clear()` drops only the in-memory cache. `PurgeDurable()` deletes `*.json` under
the active durable root (automation / recovery). Domain tools should not purge the
process store during normal operation.

## Integration

Domain modules call `FUeremcpIdempotencyStore::Get()` the same way as the
session-scoped path. No Gameplay-specific durable API is required: Put writes
through to disk when durable persistence is enabled.

## Tests

Automation filter prefix: `UEREMCP.Protocol.Idempotency`

| Test | Covers |
|---|---|
| `ReplayAnnotation` | in-memory replay annotation |
| `DurableReload` | Put → new store instance → hydrate + replay + purge |
| `DurableRoot` | accepted Saved/UEREMCP/idempotency root + stable file stems |
