# RB-10: Template and pattern library substrate

- **Owner:** WS-15
- **Status:** not_started
- **Blocks:** ADR-0008, POC C (variation from existing effect)
- **Priority:** high

## Framing

Master prompt §10 wants a general library storing asset, graph, system, gameplay,
Niagara, material, animation, Blueprint, replication, UI, and AI patterns — searchable,
versioned, inheritable, composable, promotable from successful assets, and
**authorable by the agent**.

Epic already ships something adjacent. `UAgentSkillToolset` exposes four `AICallable`
statics — `ListSkills`, `GetSkills`, `CreateSkill`, `UpdateSkill` — over `UAgentSkill`
UObject assets `[VERIFIED: $TR/.../Public/ToolsetRegistry/AgentSkill.h]`. Agent-authored,
listable, describable, updatable at runtime. That is most of the lifecycle §10 asks for.

The difference: `UAgentSkill` is **prompt-shaped** — it has
`GeneratePrompt(InitialInstructions)` returning a string. Our templates are
**execution-shaped** — `construction_plan` is an array of batch operations, plus
`validation_rules`, `typical_ranges`, and `known_failure_cases`
(`schemas/template-library/template.schema.json`).

This brief decides whether we subclass, sit beside, or ignore it. ADR-0008 follows.

## Questions

1. What is in `FAgentSkillDetails`? Where are skills stored, and how are they
   discovered?
2. Can `UAgentSkill` be subclassed to carry structured payloads, and do `ListSkills` /
   `GetSkills` still surface subclass data usefully?
3. What does `CreateSkill` actually accept, and where does it write?
4. Is there a search/similarity mechanism, or only `ListSkills`? If `SemanticSearchToolset`
   provides project-wide semantic search (RB-02 q10), can it index templates? That would
   deliver §10's "similarity matching" without building an index.
5. Does versioning/inheritance exist, or must we layer it?
6. **Prompt-shaped vs execution-shaped:** is `GeneratePrompt` a useful complement — the
   template both *executes* deterministically and *explains itself* to an agent that
   wants to improvise? Consider it seriously; it is cheap if the substrate already does it.
7. If we go bespoke, what asset type — `UDataAsset`, `UPrimaryDataAsset`, or JSON files
   in the project? Weigh source-control friendliness, hot reload, and whether agents can
   author them easily.

### Library design

8. How does a template get **promoted from a successful asset** (§5.3, §10)? Diffing a
   produced asset against the spec that produced it, and recording the delta as a
   pattern, is the interesting mechanism — is it feasible?
9. How does "create a frost projectile based on the fireball template, replace flame
   turbulence with crystalline fragments, reduce trail persistence, add a freezing
   impact decal, preserve networking and damage" become **one** operation? This is the
   master prompt's headline example — design the modifier grammar for it and validate
   it against POC C.
10. How does the library record **failures** (`known_failure_cases`)? This is what makes
    the library compound in value rather than just accumulate. What writes to it — a
    failed validation automatically, or a human/agent decision?
11. Project-specific overrides vs shared patterns — how are they layered and resolved?
12. How does a template declare engine-version compatibility, and what happens to
    templates authored on 5.8 after an engine upgrade?

## Deliverables

- [ ] A recommendation for ADR-0008, with evidence
- [ ] Working store / search / instantiate / promote, against the frozen
      `template.schema.json`
- [ ] The modifier grammar that makes question 9 one operation
- [ ] Seed templates from WS-07's emitter archetypes, proving POC C
- [ ] A written answer to question 10 — the failure-recording loop is the part most
      likely to be quietly skipped
