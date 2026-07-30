# ADR-nnnn: <short decision title>

- **Status:** Proposed | Accepted | Superseded by ADR-nnnn | Rejected
- **Date:** YYYY-MM-DD
- **Owner:** WS-nn
- **Unblocks:** <workstreams or deliverables that were waiting on this>
- **Depends on:** <research briefs, other ADRs>

## Context

What forced a decision. Include the constraints that are real (engine behaviour,
existing code, measured cost) and separate them from the constraints that are
preference. Cite evidence with verification tags — `[VERIFIED: path:line]`,
`[VERIFIED-RUNTIME: how]`, `[DOCS: url]`. An ADR may not contain `[UNVERIFIED]`
claims; if the evidence is not there yet, this ADR stays `Proposed`.

## Decision

State it in the imperative, in one or two sentences. "We will ..."

Then the specifics an implementer needs: types, names, file locations, contracts.

## Alternatives considered

For each, what it was and the concrete reason it lost. An alternative dismissed
without a reason will be re-proposed by the next agent — that is the whole cost this
section exists to prevent.

| Alternative | Why rejected |
|---|---|
| | |

## Consequences

**Enables:** what becomes possible or cheaper.

**Costs:** what becomes harder, slower, or riskier. Be honest — an ADR with no
downsides listed has not been thought through.

**Locks in:** what would be expensive to reverse later, and roughly how expensive.

## Open questions

Things this ADR deliberately does not settle, and which brief or ADR will.

## Verification

How an implementer confirms they have complied. Ideally a command, a test name, or
a schema to validate against.
