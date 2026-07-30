# Research Protocol

**Owner:** WS-01. Applies to every workstream.

## Why this is strict

The dominant failure mode on an Unreal automation project is an agent confidently
asserting that an engine API exists when it does not — or exists but is private, or
editor-only, or was removed in 5.8, or behaves differently than its name suggests.
Downstream work builds on the assertion, and the failure surfaces days later as an
unexplained compile error or a silently broken asset.

Master prompt rule 18 is the whole of it: **do not claim an Unreal API exists without
verifying it.**

## Verification tags — mandatory on every API or behaviour claim

| Tag | Means | Where allowed |
|---|---|---|
| `[VERIFIED: <path>:<line>]` | You read it in engine source or a header **on this machine** | anywhere |
| `[VERIFIED-RUNTIME: <how>]` | You executed it in the editor and observed the result | anywhere |
| `[DOCS: <url>]` | Official Epic documentation; no local confirmation | research notes, docs; **not** ADRs |
| `[UNVERIFIED]` | Inference, forum post, memory, or reasonable guess | **research notes only** |

Rules:

- An **untagged** API claim is treated as `[UNVERIFIED]` and will be rejected by WS-14.
- `[UNVERIFIED]` is **forbidden** in ADRs, schemas, and implementation comments. If
  the evidence is not there, the ADR stays `Proposed`.
- `[VERIFIED-RUNTIME]` outranks `[VERIFIED]` when they disagree. Headers describe
  intent; runtime describes reality. Record the disagreement — it is a finding.
- Prefer `[VERIFIED]` over `[DOCS]`. Epic's docs lag the engine, and 5.8's agent stack
  is Experimental and largely undocumented.

Being wrong with a tag is recoverable — someone can check your citation. Being wrong
without one is not, because nobody can tell which claims to re-check.

## Source priority

1. Local engine headers and source — `$UE_ROOT/Engine/`
2. Runtime experiment in the editor (Python console, an automation test, a temporary tool)
3. Local plugin/project source — `REAgentTools`, `RE/Source`, `RECore`
4. Official Epic documentation and the C++ API reference
5. Epic sample plugins and in-engine reference implementations
   (`UAgentSkillToolset` is the canonical `AICallable` example)
6. Everything else — forums, blogs, third-party MCP projects. Useful for *ideas*,
   never as evidence.

`docs/GROUNDED_FACTS.md` is a curated tier-1 summary. Cite it directly instead of
re-deriving what it already establishes.

## Record negative findings

A negative result is a first-class deliverable. "There is no public API to
enumerate Niagara module stack entries; here is what I tried and where I looked" is
**more valuable** than silence, because it stops fourteen agents repeating the search
and it forces the architectural conversation early.

Every brief has a "Negative findings" section. An empty one on a hard question is
itself suspicious.

## Brief structure

Your brief lives at `docs/research/RB-<nn>-<slug>.md`. You own it. Structure:

```markdown
# RB-nn: <topic>

- **Owner:** WS-nn
- **Status:** not_started | in_progress | complete | blocked
- **Blocks:** <what cannot proceed without this>
- **Last updated:** YYYY-MM-DD

## Questions
The specific questions this brief must answer. Copy from the assignment; add
questions you discover, do not silently drop ones you could not answer.

## Findings
One subsection per question. Every claim tagged. Include the code you ran and the
output you saw for [VERIFIED-RUNTIME] claims.

## Negative findings
What does not exist, is not reachable, or does not work as its name implies.

## API availability summary
| API / capability | Public | Editor-only | C++ | Python | Notes | Tag |
|---|---|---|---|---|---|---|

## Architectural implications
What this means for the design. Flag anything that contradicts an accepted ADR — and
raise it via docs/proposals/ rather than editing the ADR.

## Open questions
What you could not resolve, and what it would take.
```

## Answer the question you were asked

A brief that surveys a domain broadly but does not answer its assigned questions is
not complete. If a question turns out to be malformed, say so explicitly and answer
the question that should have been asked — do not quietly substitute it.

## When you cannot verify

Say so plainly and move on to what you can verify. Do not fill the gap with a
plausible guess — a plausible guess is indistinguishable from a finding once it is
written down, and that is precisely how this project fails.

If a blocker changes the architecture, that is an escalation: write
`docs/proposals/<your-ws>-<topic>.md` and flag it. Recording a blocker precisely is
worth more than working around it silently.
