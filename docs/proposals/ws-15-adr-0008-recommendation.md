# Proposal: ADR-0008 — Template & pattern library substrate

- **From:** WS-15
- **To:** WS-01 (ADR author)
- **Date:** 2026-07-29
- **Status:** ready for ADR drafting
- **Evidence:** [`docs/research/RB-10-template-substrate.md`](../research/RB-10-template-substrate.md)

## Recommended decision

**Structured adjacent asset.** Canonical templates are JSON documents conforming to
`schemas/template-library/template.schema.json`, stored in repo `templates/` and
optionally overridden under `/Game/__UeremcpTemplates/`. The `UeremcpTemplates`
module loads, validates, indexes, and serves them via three goal-level actions:
`search_templates`, `instantiate_template`, `promote_to_template`.

**Do not subclass `UAgentSkill` as the execution substrate.**

**Compose with Epic primitives** where they fit; do not duplicate them.

## Rationale (evidence summary)

### Why not `UAgentSkill`

1. **Shape mismatch.** `UAgentSkill` exposes `Description`, `Instructions`, and
   `GeneratePrompt` — prompt-shaped
   `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h:34-48]`.
   UEREMCP templates require `construction_plan` (batch operations),
   `validation_rules`, `inputs`, `supported_modifiers`
   `[VERIFIED: schemas/template-library/template.schema.json]`.

2. **API ceiling.** `FAgentSkillDetails` contains only `Instructions: FString`
   `[VERIFIED: AgentSkill.h:17-26]`. `GetSkills` returns this struct exclusively
   `[VERIFIED: AgentSkill.cpp:60-85]`. Subclass UPROPERTY fields are not surfaced.

3. **Missing lifecycle features.** No versioning, inheritance, modifier grammar, or
   search in Epic's skill model
   `[VERIFIED: AgentSkill.h, AgentSkill.cpp:26-57]`.

4. **Subclassing is the wrong coupling.** Stuffing JSON into `Instructions` forfeits
   schema validation at the Epic layer and conflates prompt text with executable plan.

### Why JSON adjacent asset

1. **Schema already frozen** — `template.schema.json` is WS-01 owned and validated by
   `tools/validate_schemas.py`.
2. **Single executor** — `construction_plan` items reference
   `batch/plan.schema.json#/$defs/operation`; no second interpreter.
3. **Source control** — templates diff in PRs; `authored_by` provenance is reviewable.
4. **`templates/README.md` already assumes this layout** pending ADR-0008.

### Optional complement: `UAgentSkill`

Generate companion Blueprint skills for frequently used templates where
`GeneratePrompt` adds agent-facing explanation. The skill references `template_id`;
execution stays in `instantiate_template`. **Not required for v1 or POC C.**

### `SemanticSearchToolset` role

**Compose, not replace.** Use for asset-side similarity (`FindSimilar` on promoted
effects) with `PathRegexes` filters
`[VERIFIED: SemanticSearchToolset.h:55-79]`. Primary template discovery is
`search_templates` over metadata — JSON templates are not in the SemanticSearch
index
`[VERIFIED: SemanticSearchToolset.h:47-48]`.

## ADR-0008 decision text (proposed for WS-01)

### Context

Master prompt §10 and §5.3 require a reusable pattern library: searchable, versioned,
inheritable, promotable, agent-authorable. Epic ships `UAgentSkillToolset` for
prompt-shaped agent skills. UEREMCP templates are execution-shaped.

### Decision

1. **Canonical store:** JSON files conforming to `schemas/template-library/template.schema.json`.
2. **Runtime:** `UeremcpTemplates` module — load, validate, index, serve. No dependency
   on subclassing `UAgentSkill`.
3. **Execution path:** `instantiate_template` materialises `construction_plan` into
   `execute_plan` (WS-05).
4. **Search:** `search_templates` over template metadata; `SemanticSearchToolset` optional
   for asset discovery during promotion.
5. **Versioning:** `template_id` suffix `.v<n>`, integer `version`, `inherits_from`,
   `deprecated_by`, ADR-0006 `expected_revision` on template mutations.
6. **Elemental and cross-pattern variation:** parameterized `inputs` (e.g.
   `element: fire|water|wind|earth`) plus named `modifiers` — not separate tools per
   element.
7. **Agent-authored templates:** quarantine path under `/Game/__UeremcpTemplates/agent/`;
   `promote_to_template` defaults `dry_run: true`; coordinate permission model with
   ADR-0010 (WS-12).
8. **Failure compounding:** `known_failure_cases` auto-append on validation failure
   (quarantine templates only).

### Alternatives rejected

| Alternative | Reason |
|---|---|
| Subclass `UAgentSkill` | API and shape mismatch (see above) |
| `UAgentSkill` as sole store with JSON in `Instructions` | Unvalidated, opaque, breaks tool discovery |
| `UPrimaryDataAsset` as canonical | Schema duplication; JSON is already frozen |
| Duplicate `UAgentSkillToolset` CRUD | Rule 2 audit — preserve Epic toolset |
| SemanticSearch as sole template index | JSON templates not indexed |

### Consequences

- WS-15 owns loader, index, `search_templates`, `instantiate_template`,
  `promote_to_template`.
- WS-01 may need `schemas/domains/project/instantiate_template.schema.json` for
  modifier grammar (WS-15 proposes shape in RB-10).
- WS-07 provides seed emitter archetypes for `templates/niagara/`.
- POC C blocked until WS-05 + WS-07 gates close.

### Verification

- `Template.LoadValidateAll` — every file in `templates/` passes schema validation.
- `Template.InheritMerge` — child overrides parent; `inherits_from` chain resolves.
- `Template.InstantiateIdempotent` — same inputs/modifiers/target → `no_change_required`.
- `Template.ThirdGeneration` — POC C7 scenario (post WS-07).

## Schema extension request (WS-01)

Add `schemas/domains/project/instantiate_template.schema.json` with:

- `template_id` (required)
- `inputs` (object)
- `modifiers` (object with `replace`, `adjust`, `add`, `preserve` arrays)
- `target`, `mode`, `idempotency_key`, `expected_revision` (from envelope patterns)

Add `schemas/domains/project/search_templates.schema.json` and
`schemas/domains/project/promote_to_template.schema.json`.

WS-15 will not edit `schemas/**` directly per ownership — this proposal is the handoff.

## Response

**Accepted.** ADR-0008 written and Accepted. Structured adjacent JSON substrate;
no `UAgentSkill` subclass. Elemental variation is parameterized inputs/modifiers.

Specification schemas added under `schemas/domains/templates/`
(`instantiate_template`, `search_templates`, `promote_to_template`). Implementation
remains gated on WS-03 host proof, WS-05 `execute_plan` wiring, and WS-07 seed
patterns — research-complete only for now.
