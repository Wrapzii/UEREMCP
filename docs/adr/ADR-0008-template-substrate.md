# ADR-0008: Template & pattern library substrate

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-15 implementation, POC C design, elemental variation templates
- **Depends on:** ADR-0003, ADR-0004, ADR-0006, RB-10
  (`docs/research/RB-10-template-substrate.md`),
  `docs/proposals/ws-15-adr-0008-recommendation.md`

## Context

Master prompt §10 and §5.3 require a reusable pattern library: searchable,
versioned, inheritable, composable, promotable from successful assets, and
authorable by the agent.

Epic ships `UAgentSkill` / `UAgentSkillToolset` — four `AICallable` tools over
prompt-shaped skill assets (`Description`, `Instructions`, `GeneratePrompt`)
`[VERIFIED: AgentSkill.h:34-104]`. `FAgentSkillDetails` carries only an
`Instructions` string `[VERIFIED: AgentSkill.h:17-26]`. `GetSkills` returns that
struct exclusively `[VERIFIED: AgentSkill.cpp:60-85]`. `ListSkills` enumerates
derived classes with no search `[VERIFIED: AgentSkill.cpp:26-57]`.

UEREMCP templates are **execution-shaped**: frozen
`schemas/template-library/template.schema.json` requires `construction_plan`,
`validation_rules`, `inputs`, and related structured fields — not a prompt string.

`SemanticSearchToolset` indexes Blueprint/Material/Texture-class assets, not JSON
template files `[VERIFIED: SemanticSearchToolset.h:47-48]`.

## Decision

We will use a **structured adjacent asset**, not a `UAgentSkill` subclass.

1. **Canonical store.** JSON documents conforming to
   `schemas/template-library/template.schema.json`, in repo `templates/` with
   optional project overrides under `/Game/__UeremcpTemplates/`.
2. **Runtime.** `UeremcpTemplates` loads, validates, indexes, and serves templates.
   No dependency on subclassing `UAgentSkill`.
3. **Agent-facing actions.** `search_templates`, `instantiate_template`,
   `promote_to_template`. Specifications live under
   `schemas/domains/templates/`.
4. **Execution path.** `instantiate_template` resolves inheritance, applies
   `inputs` + named `modifiers`, materialises `construction_plan`, and delegates
   to `execute_plan` (WS-05). One interpreter — no second plan engine.
5. **Search.** Primary: `search_templates` over template metadata.
   Compose `SemanticSearchToolset` optionally for **produced-asset** similarity
   during promotion — not as the template index.
6. **Versioning.** `template_id` with `.v<n>` suffix, integer `version`,
   `inherits_from`, `deprecated_by`. Mutations honour ADR-0006
   `expected_revision` / idempotency.
7. **Elemental and cross-pattern variation.** Parameterized `inputs` (e.g.
   `element: fire|water|wind|earth`) plus named `modifiers` — **not** separate
   tools or templates per element for the same pattern family.
8. **Agent-authored templates.** Quarantine under
   `/Game/__UeremcpTemplates/agent/`. `promote_to_template` defaults
   `dry_run: true`. Permission tiers coordinate with ADR-0010 (WS-12).
9. **Optional complement.** Companion `UAgentSkill` assets may explain a
   `template_id` via `GeneratePrompt`. Execution stays in `instantiate_template`.
   Not required for v1 or POC C.
10. **Preserve Epic.** Do not duplicate `UAgentSkillToolset` CRUD
    (`docs/proposals/ws-15-audit-rows.md`).

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Subclass `UAgentSkill` as substrate | API returns only `Instructions`; structured plan/validation fields invisible to Epic tools. Shape mismatch. |
| JSON stuffed into `Instructions` | Unvalidated at Epic layer; opaque; conflates prompt with executable plan. |
| `UPrimaryDataAsset` as canonical store | Duplicates the frozen JSON schema; worse PR diffs. Optional editor wrappers later are fine. |
| Duplicate `UAgentSkillToolset` | Violates audit rule 2 — preserve Epic. |
| `SemanticSearchToolset` as sole template index | Does not index JSON template files. |

## Consequences

**Enables:** POC C variation (including third-generation instantiation), elemental
presets over shared Niagara/material/ability patterns, agent promotion into
quarantine, schema-validated template PRs.

**Costs:** UEREMCP owns template search/index; Epic skill tools remain for
prompt-shaped work only. `instantiate_template` is blocked until `execute_plan`
and domain construction (WS-07 POC B for Niagara seeds) exist.

**Locks in:** JSON + `execute_plan` as the template execution path. Reversing to
`UAgentSkill` subclassing would discard structured validation and force an
Epic API fork.

## Open questions

1. Exact on-disk layout for project overrides vs repo seeds (WS-15 impl).
2. Whether a thin `UPrimaryDataAsset` editor wrapper is worth Wave 3 UX
   (optional; not substrate).
3. ADR-0010 permission tiers for agent quarantine writes (WS-12).

## Verification

- `Template.LoadValidateAll` — every file under `templates/` validates.
- `Template.InheritMerge` — `inherits_from` chain resolves; child overrides parent.
- `Template.InstantiateIdempotent` — same inputs/modifiers/target →
  `no_change_required` or replayed idempotent response.
- `Template.ThirdGeneration` — POC C7 after WS-07 seed templates exist.
