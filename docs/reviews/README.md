# Reviews

**Owner:** WS-14, the Integration Critic. WS-14 writes only here and produces no
implementation.

## The job

Master prompt §13.14: review all work and identify conflicting assumptions, missing
integration points, engine-version incompatibilities, unsupported claims, weak
validation, excessive low-level tools, duplicate functionality, poor JSON design, and
agent-unfriendly interfaces.

With fifteen agents producing in parallel, the failure mode is not any single bad
deliverable — it is fifteen individually reasonable deliverables that do not compose.
WS-14 exists to catch that, and it runs **continuously from Wave 1**, not as a gate at
the end.

## Review checklist

Run against every deliverable claiming completion:

**Claims**
- [ ] Every API claim carries a verification tag. Untagged is `[UNVERIFIED]`.
- [ ] No `[UNVERIFIED]` claims in ADRs, schemas, or implementation comments.
- [ ] No claim of success without re-read-and-verify evidence.
- [ ] Limitations are documented, not omitted. Silence about a limitation is a defect.

**Duplication** (R-06 — the most likely way this project wastes effort)
- [ ] `docs/audit/` records the Epic equivalent and why it is insufficient.
- [ ] No tool duplicates a working REAgentTools toolset without a stated reason.
- [ ] Domain workstreams are not each reimplementing shared machinery — graph
      serialisation, hashing, validation, `$ref` resolution.

**Altitude**
- [ ] The agent-facing surface is goal-level, not primitive.
- [ ] Internal primitives are hidden via `SetNameFilters`, not exposed.
- [ ] No tool forces an inspect → mutate → inspect loop.

**Protocol conformance**
- [ ] Conforms to frozen schemas; `tools/validate_schemas.py` passes.
- [ ] The envelope was not extended — only `specification`.
- [ ] `metrics.mcp_round_trips` and `internal_operations` are populated.
- [ ] `response_detail` is honoured; `summary` does not carry bulk payloads.
- [ ] Statuses are honest — no `*_validated` where validation was skipped.

**Integration**
- [ ] Cross-workstream contracts are agreed by both sides, not assumed by one.
      (Cue↔VFX, montage↔ability, template↔domain are the usual failure points.)
- [ ] The handoff artifact in `docs/WORK_ALLOCATION.md` exists.
- [ ] No accepted ADR was quietly contradicted.

**Tests**
- [ ] Tests exist and pass, or the reason they cannot run is stated.
- [ ] Rollback, idempotency, and revision-conflict tests are not skipped.

## Files

| File | Contents |
|---|---|
| `poc-metrics.md` | Measured metrics for every POC — calls, tokens, wall clock, completion rate |
| `<ws-nn>-<date>.md` | Review of a specific deliverable |
| `integration-log.md` | Running log of cross-workstream conflicts found and resolved |

## How to write a review

Be specific and falsifiable. "The Niagara specification schema does not express
`compile_policy`, so a batch containing it cannot use `at_boundaries`" is actionable.
"The Niagara design feels underspecified" is not.

Findings that block go to `docs/proposals/` as well, addressed to the owning workstream
— a review nobody is obliged to read changes nothing.

## The one thing worth catching above all others

**A false claim of success.** Everything else is recoverable; a `created_and_validated`
on something that was never validated poisons every decision downstream of it and is
invisible until much later. Check the evidence, not the status field.
