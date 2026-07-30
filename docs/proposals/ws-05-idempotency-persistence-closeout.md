# WS-05 / WS-11: idempotency persistence hardening closeout

- **Branch:** `ws-05-idempotency-persistence-hardening`
- **Base:** local `main` at `227cb9914b1fceec66498f4f74293177ad4b225b`
- **Date:** 2026-07-30
- **Status:** complete with runtime evidence

## Capability catalog update requested from WS-01

Add the following guarantees to the `execute_plan` capability:

- durable pre-mutation idempotency claim under `Saved/UEREMCP/idempotency`
- exact request fingerprint binding, excluding retry-only IDs
- completed replay across editor restart with zero domain work
- conflicting key reuse and concurrent same-key work rejected before mutation
- atomic metadata replacement, fail-closed corruption quarantine, seven-day retention
- durable-commit failure surfaced as `partially_completed`

No envelope or domain schema fields changed.

## Root cause and resolution

`Idempotency.RepeatedCreate` mixed a session harness with the default durable process
store: `Clear()` intentionally preserved disk, while the fixed key could hydrate a
prior run. The harness now injects an isolated memory store. Restart durability is
tested separately in two editor processes.

Production `execute_plan` now uses `Claim` / `Complete`. The durable v2 record binds
the key to a request fingerprint, reserves it before work, and rejects conflicting or
already-running reuse.

## Verification matrix

| Check | Result | Evidence |
|---|---|---|
| Protocol Python suite | PASS | local `run_tests.py` |
| Schema validation | PASS | `python tools/validate_schemas.py` |
| Isolated plugin package build | PASS | `UEREMCP-ws05-idempotency-build` |
| `UEREMCP.Protocol.Idempotency` (5 tests) | PASS | `editor_UEREMCP_Protocol_Idempotency_20260730_144519.log` |
| `UEREMCP.Validation.Idempotency.RepeatedCreate` | PASS | `editor_UEREMCP_Validation_Idempotency_RepeatedCreate_20260730_144559.log` |
| `UEREMCP.Validation.Revision.StaleRejected` | PASS | `editor_UEREMCP_Validation_Revision_StaleRejected_20260730_144619.log` |
| Restart Create | PASS | `editor_UEREMCP_Validation_Idempotency_Restart_Create_20260730_145027.log` |
| Restart Verify (fresh editor) | PASS | `editor_UEREMCP_Validation_Idempotency_Restart_Verify_20260730_145155.log` |

Root cause of the CurveFloat flake was confirmed by a leftover durable v1 record for
`idem-repeated-create-v1` under `Saved/UEREMCP/idempotency/`.

## Limitations

- Metadata and package files are not one atomic transaction. FileSandbox/editor
  transactions remain authoritative for asset rollback.
- Crash-after-mutation-before-completion leaves an in-progress claim for one hour.
  Reclaim depends on stable paths plus domain no-op/revision checks.
- Legacy domain call sites still using `Put` / `TryGetReplay` do not gain fingerprint
  conflict detection until their owning workstreams migrate to `Claim` / `Complete`.
  `execute_plan` is migrated in this branch.
