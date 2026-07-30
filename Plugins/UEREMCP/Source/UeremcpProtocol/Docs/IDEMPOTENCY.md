# Idempotency store (ADR-0006)

**Owner:** WS-05  
**Authority:** `docs/adr/ADR-0006-idempotency-revisions.md`,
`docs/proposals/ws-12-idempotency-store-root.md`

## Behaviour

`FUeremcpIdempotencyStore` records
`(idempotency_key, request_fingerprint → response JSON)` on verified completion.
Production callers use `Claim` before mutation and `Complete` afterward:

- same key and exact semantic request: replay with `metrics.replayed: true`
- same key and different request: conflict, no mutation
- same key while its first request is active: in-progress, no duplicate mutation
- absent/expired key: atomically reserve it before mutation

Request fingerprints remove retry-only `request_id` and `idempotency_key`; all other
fields, including `expected_revision`, remain significant.

Empty keys are ignored.

## Durability

Wave 1 in-memory caching remains. Wave 2 disk durability is enabled by default on
each store instance:

```
<ProjectSavedDir>/UEREMCP/idempotency/<sha256(key)>.json
```

Never under `Intermediate/Sandboxes/` — sandbox Discard must not erase retry
records `[VERIFIED: docs/proposals/ws-12-idempotency-store-root.md]`.

Each durable file is a v2 record containing the original key, exact request
fingerprint, lifecycle state, timestamps, and (when complete) response object.
Writes use a same-directory temporary file followed by replacement. A named
system-wide mutex serializes record transitions across editor processes
`[VERIFIED: WindowsPlatformMutex.h:119-140; FileManager.h:112-113]`.

Unreadable records fail closed and are renamed with a `.corrupt.<timestamp>` suffix
for diagnosis. Completed records expire after seven days by default. In-progress
claims may be reclaimed after one hour; callers can override both durations in
isolated tests.

`Clear()` drops only the in-memory cache. `Remove(key)` deletes one plugin-owned
record. `PurgeDurable()` deletes `*.json` under the active durable root
(isolated automation / recovery). Domain tools must not purge the process store
during normal operation.

## Integration

`execute_plan` computes the request fingerprint, claims before execution, and
completes only after a terminal response. Failure to commit the durable completion
is surfaced as `partially_completed`; it is never silently reported as retry-safe.

Legacy `Put` / `TryGetReplay` remain for compatibility and isolated harnesses.
Records written without a request fingerprint are deliberately not claimable by
the production path, because conflicting reuse cannot be proven safe.

## Tests

Automation filter prefix: `UEREMCP.Protocol.Idempotency`

| Test | Covers |
|---|---|
| `ReplayAnnotation` | in-memory replay annotation |
| `DurableReload` | Put → new store instance → hydrate + replay + purge |
| `DurableRoot` | accepted Saved/UEREMCP/idempotency root + stable file stems |
| `ClaimConflictRestart` | claim, in-progress exclusion, atomic complete, restart replay, conflict |
| `CorruptionAndExpiry` | fail-closed quarantine and retention cleanup |

Two-process editor pair:

```
pwsh tests/run_idempotency_restart.ps1
```

The create process persists a completed replay record and then changes a CurveFloat
out of band. The verify process replays without mutation, rejects conflicting key
reuse, and rejects the stale asset-derived revision after rereading the asset.

## Limitations

- Atomicity covers each metadata record, not the Unreal asset package and metadata
  as one filesystem transaction. FileSandbox/transactions remain the asset rollback
  boundary; the idempotency claim intentionally lives outside FileSandbox.
- A process crash after asset mutation but before `Complete` leaves an in-progress
  claim. It blocks duplicate work until its one-hour stale threshold. Reclaim after
  that threshold relies on stable paths and domain no-op/revision checks.
- Cross-process exclusion is implemented on platforms where Unreal provides a
  working system-wide mutex. The current runtime target is Windows.
