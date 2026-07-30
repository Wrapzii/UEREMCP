# Guides

**Owner:** WS-13. Wave 3 — mostly unwritten, deliberately.

Documentation written before the implementation exists documents a guess. These are
placeholders describing what each guide must cover, so WS-13 knows the shape and nobody
mistakes an empty directory for an oversight.

## Planned guides

| Guide | Audience | Must cover |
|---|---|---|
| `agent-usage.md` | AI agents consuming UEREMCP | How to discover actions, the envelope, `response_detail` selection, revision handling, batching, reading diagnostics |
| `capability-reference.md` | agents + humans | Every action with its `specification` schema and a worked example. Generated from `schemas/domains/` where possible — a hand-maintained reference will drift |
| `template-authoring.md` | humans + agents | Writing and promoting templates. Extends `templates/README.md` |
| `developer-setup.md` | humans | Building the plugin, enabling it, connecting a client, running tests |
| `troubleshooting.md` | both | Failure modes and what they mean |
| `limitations.md` | both | **What UEREMCP cannot do.** Aggregated from every workstream's documented ceiling |

## Two notes for WS-13

**`limitations.md` is the most important file here.** Master prompt rule 19 requires
documenting unavoidable limitations, and `AGENTS.md` rule 6 makes an undocumented
limitation a defect. Every workstream produces a capability ceiling — RB-09 will very
likely find Control Rig is read-only, RB-05 may find node types that cannot be
reconstructed. Aggregate them honestly. An agent that knows a boundary works around it;
an agent that does not, fails against it repeatedly and expensively.

**Write `agent-usage.md` for the actual consumer.** The readers are Grok, Composer, and
Claude agents, not humans skimming. That means: complete worked examples over prose,
explicit statements of what each field does, and no "see also" chains — under this
project's cost model (`docs/WHY.md`), making the reader fetch a second page is the same
mistake as making an agent issue a second tool call.
