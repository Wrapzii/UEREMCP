# Proposals

**Anyone may write here.** This is the escape hatch that makes strict file ownership
workable.

## When to write a proposal

- You need a change in a path another workstream owns (`AGENTS.md` rule 3)
- You believe a frozen ADR is contradicted by engine reality (rule 4)
- Two workstreams need the same schema change
- A required Unreal API turns out not to exist, and the workaround changes the design
- An operation cannot be made verifiable

**Write the proposal and keep working on what you do own.** Do not block. Do not edit
the other path anyway.

## Naming

`<your-ws>-<topic>.md`, lowercase. ADR challenges: `<your-ws>-adr-<nnnn>-challenge.md`.

```
ws-07-batch-schema-niagara-compile-boundary.md
ws-06-adr-0004-challenge.md
```

## Structure

```markdown
# <ws-nn>: <what you need>

- **To:** WS-nn (the owner)
- **Blocks:** what you cannot do without it, or "nothing — working around it"
- **Date:** YYYY-MM-DD

## What I need
Specific. A diff or exact wording beats a description.

## Why
Evidence, with verification tags. An ADR challenge without [VERIFIED] engine evidence
will not be actioned.

## What breaks without it
Be honest — "nothing, this is a preference" is a legitimate answer and saves everyone
time.

## What I am doing meanwhile
The workaround you have adopted so you are not blocked.
```

## What gets actioned quickly

A proposal backed by `[VERIFIED]` or `[VERIFIED-RUNTIME]` evidence that something does
not work as designed. That is the signal this whole system exists to surface.

A proposal backed by preference, or by "I would have designed it differently," will
not be — the cost of fifteen agents each improving the protocol slightly is a protocol
that is not a protocol.

## Resolution

The owning workstream replies inline under `## Response` and updates the status line.
Resolved proposals stay in the directory — the reasoning trail is worth more than a
tidy folder.
