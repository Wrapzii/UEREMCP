# Proposal: expected_revision on envelope and per batch operation

- **From:** WS-05
- **To:** WS-01
- **Date:** 2026-07-29
- **Addresses:** ADR-0003 open question — "Whether `expected_revision` belongs in
  the envelope or per-operation inside batches"

## Recommendation

**Keep both.** No ADR change required.

| Surface | Field | Role |
|---|---|---|
| Envelope (`request.schema.json`) | `expected_revision` | Single-asset / single-graph optimistic concurrency (ADR-0006 rule 4) |
| Batch operation (`plan.schema.json`) | `operations[].expected_revision` | Per-asset guard inside `execute_plan`, where one request touches many assets |

## Rationale

A batch that creates a material then patches a Blueprint cannot express both
guards with one envelope-level revision. Envelope-level remains correct for
non-batch tools. Per-operation remains correct for multi-asset plans. They are
not alternatives; they are different scopes.

Default conflict policy stays envelope `options.on_revision_conflict` =
`reject`. Batch-level override of that policy is out of scope until the batch
grammar is unblocked.
