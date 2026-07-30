# ADR-0006: Idempotency, revisions, and conflict handling

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-05 (protocol), all domain workstreams
- **Depends on:** ADR-0003, ADR-0004

## Context

Master prompt §2.6 requires create-or-update semantics with no uncontrolled
duplicates, and §17 requires that modifications based on stale graph state be
rejectable.

Both matter more than usual here, because agents retry. An agent that times out, or
loses its result, or is one of several agents working the same project, will re-issue
a request. If `create_spell "Fireball"` produces `NS_Fireball`, `NS_Fireball_1`,
`NS_Fireball_2` across three retries, the system is unusable in exactly the
multi-agent setting it is being built for.

## Decision

Four mechanisms, layered.

**1. Stable asset paths are the primary identity.** A request's `target.asset_path`,
or a path deterministically derived from `specification.name` plus the domain's
naming convention, identifies the asset. The same logical request always resolves to
the same path. Naming conventions are declared per domain in
`schemas/domains/<domain>/` and must be pure functions of the specification.

**2. `mode` governs collision behaviour** — explicitly, never by guessing:

| `mode` | Target exists | Target absent |
|---|---|---|
| `create` | `rejected` — no silent suffixing | create |
| `create_or_update` | update in place | create |
| `replace` | delete contents, rebuild | `rejected` |
| `patch` | apply patch | `rejected` |
| `rebuild_from_specification` | rebuild in place | create |
| `repair` | analyse and fix | `rejected` |
| `delete` | delete (honours `dry_run`) | `no_change_required` |

Auto-suffixing a name to dodge a collision is prohibited. It is the mechanism by
which duplicate-asset sprawl happens.

**3. `idempotency_key` deduplicates retries.** When present, the service records
`(idempotency_key → response)` on completion. A repeat within the retention window
returns the **stored response** with `metrics.replayed: true`, and performs no work.
Retention window and store location are WS-05's to specify; in-memory for the editor
session is the minimum bar, surviving editor restart is preferred.

**4. `expected_revision` guards against stale state.** Every graph and asset
retrieval returns `revision` and `content_hash` (ADR-0004). A modifying request may
carry `expected_revision`. If it does not match current state, `options.on_revision_conflict`
decides:

| Value | Behaviour |
|---|---|
| `reject` (**default**) | `status: rejected`, return current `revision` and a diff summary |
| `return_conflict` | as `reject`, plus the current full state so the agent can merge without another round trip |
| `merge` | attempt semantic merge; `rejected` if ambiguous. **Not required for v1.** |
| `replace` | overwrite regardless |
| `force` | overwrite and skip all conflict checks |

Default is `reject`. Silent last-writer-wins is not acceptable when several agents
share a project.

**Additionally:** operations should detect and report no-ops. If the specification
already matches reality, return `no_change_required` rather than rewriting and
re-compiling identical content. This is what makes retry cheap rather than merely
safe.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Auto-suffix on collision (`_1`, `_2`) | Produces exactly the duplicate sprawl §2.6 prohibits, and hides agent bugs. |
| Idempotency by hashing the whole request | Cannot distinguish "retry" from "deliberately do it again"; breaks when a timestamp or `request_id` is in the payload. Explicit key is unambiguous. |
| Revision as monotonic integer | Requires a server-side counter surviving restarts. A content hash is derivable from state and needs no bookkeeping. Integer counter can be layered on later if wanted. |
| `force` as default | Convenient for single-agent use, actively dangerous for a swarm. Available, not default. |
| No conflict checking in v1 | The user's stated deployment is many concurrent agents. This is the case it must handle. |

## Consequences

**Enables:** safe retries, safe concurrency between multiple agents in one project,
and cheap no-op detection so repeated goal-level requests do not thrash compilation.

**Costs:** every domain service must implement a deterministic path derivation and a
meaningful no-op comparison — the latter is real work, since "does this Niagara system
already match this specification?" is a nontrivial comparison. Domains may ship
`no_change_required` detection in a later pass, but must then say so rather than
report `modified_and_validated` for a no-op.

**Locks in:** `content_hash` semantics, shared with ADR-0004. If hashing changes,
every stored `expected_revision` and every template's recorded hash is invalidated.

## Open questions

- ~~Where does the idempotency store live, and does it survive editor restart?~~
  **Closed for v1:** in-memory, process-scoped, editor-session lifetime
  (`docs/proposals/ws-05-idempotency-store.md`). Durable store deferred to
  WS-03 lifecycle + WS-12 writable-root.
- ~~What is `content_hash` computed over?~~ **Closed for protocol v1:**
  `Plugins/UEREMCP/Source/UeremcpProtocol/Docs/CONTENT_HASH.md`. Ignores layout /
  GUIDs / retrieval metadata; sensitive to pin defaults, links, node properties.
- Should `revision` cover the asset's dependencies too, so that a changed dependency
  invalidates a cached graph? Probably yes for graphs; cost unknown. (`RB-05`)

## Verification

Test `Idempotency.RepeatedCreate`: issue the same `create_or_update` request three
times. Assert exactly one asset exists, the path is identical each time, the second
and third return `no_change_required` or a replayed response, and no compile was
triggered on the repeats.

Test `Revision.StaleRejected`: retrieve a graph, modify the asset out-of-band, submit
with the original `expected_revision`, assert `status: rejected` with the current
revision returned and **no mutation performed**.
