# Architecture Decision Records

**Owner:** WS-01 (Lead Architect). No other workstream edits files in this directory.

## Why ADRs on this project

Fourteen agents working concurrently will independently invent fourteen incompatible
protocols unless the shared decisions are written down and frozen first. An ADR
records *what was decided, when, on what evidence, and what was rejected* — so a
later agent reading the code does not "fix" a deliberate choice.

## Status values

| Status | Meaning |
|---|---|
| `Proposed` | Drafted, open for challenge. Do not implement against it yet. |
| `Accepted` | **Frozen.** Implement against it. Challenge only via `docs/proposals/`. |
| `Superseded by ADR-nnnn` | Historical. Kept for the reasoning trail. |
| `Rejected` | Considered and declined. Kept so it is not re-proposed. |

## Index

| ADR | Title | Status |
|---|---|---|
| [0001](ADR-0001-engine-baseline.md) | Engine baseline and substrate | Accepted |
| [0002](ADR-0002-host-model.md) | Host model: in-process toolset plugin, not a new external server | Accepted |
| [0003](ADR-0003-request-response-envelope.md) | Versioned JSON request/response envelope | Accepted |
| [0004](ADR-0004-graph-representation.md) | Complete graph representation and exchange | Accepted |
| [0005](ADR-0005-transactions-rollback.md) | Transactions, sandboxing, and rollback | Accepted |
| [0006](ADR-0006-idempotency-revisions.md) | Idempotency, revisions, and conflict handling | Accepted |
| 0007 | Implementation language for domain services (C++ vs Python split) | **Unwritten** — needs RB-03 |
| [0008](ADR-0008-template-substrate.md) | Template & pattern library substrate | **Accepted** |
| [0009](ADR-0009-long-running-jobs.md) | Long-running job model (progress, cancellation, resumption) | **Accepted** |
| 0010 | Security model and permission tiers | **Unwritten** — needs RB-13 |

ADR-0008 is written from RB-10. ADR-0009 is written from RB-04. ADRs 0007 and
0010 stay unwritten until their blocking briefs land — **WS-01 writes them then**;
no other workstream should assume an answer in the meantime.

## Challenging a frozen ADR

1. Write `docs/proposals/<your-ws>-adr-<nnnn>-challenge.md`.
2. State the evidence — with verification tags per `AGENTS.md` rule 1.
3. State what breaks if the ADR stands.
4. **Continue working against the accepted ADR** while the challenge is open.

A challenge backed by `[VERIFIED]` engine evidence will be actioned quickly. A
challenge backed by preference will not.

## Template

Copy [`_TEMPLATE.md`](_TEMPLATE.md).
