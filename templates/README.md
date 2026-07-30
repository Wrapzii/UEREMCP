# Template & Pattern Library

**Owner:** WS-15. Schema: [`schemas/template-library/template.schema.json`](../schemas/template-library/template.schema.json).
Brief: [RB-10](../docs/research/RB-10-template-substrate.md).

## What a template is — and is not

**Not** a copied asset. A template is a **construction pattern**: an ordered,
machine-executable `construction_plan` (batch operations), plus the parameter schema it
accepts, the validation rules an instance must pass, sane value ranges, a performance
profile, and — the field that makes the library compound in value — the failures it has
actually produced before.

The test of whether you built a template library or a duplicates folder is POC C
criterion C7: **can a template instantiated from a promoted asset produce a third,
different variation?** If not, it is a copy.

## Storage substrate

ADR-0008 is accepted: canonical templates are JSON documents in this directory,
loaded by `UeremcpTemplates`, and instantiated through the shared `execute_plan`
interpreter. Epic `UAgentSkill` remains prompt-shaped and is not the execution
substrate `[VERIFIED: AgentSkill.h:34-104; ADR-0008]`.

Domain templates conform to `template.schema.json`. Element value documents conform
to `schemas/domains/templates/element_preset.schema.json`; the template service loads
them as data and injects the selected preset while materializing a plan.

## Layout

```
templates/
  elements/      element.fire.v1.json, ... (data for one parameterized family)
  niagara/       niagara.projectile.fireball.v1.json, ...
  materials/
  blueprints/
  gameplay/
  animation/
```

`template_id` follows `<domain>.<category>.<name>.v<n>` and must match the filename.
Element preset ids follow `element.<name>.v<n>` and also match the filename.

## Authoring rules

1. **Validate against the schema.** `python tools/validate_schemas.py`.
2. **`construction_plan` steps are batch operations** — they conform to
   `schemas/batch/plan.schema.json#/$defs/operation`. A template is therefore executable
   by the same engine that runs `execute_plan`, with no second interpreter.
3. **Compose, do not duplicate.** Use `inherits_from` and `composes`. A frost projectile
   inheriting from a fireball with three modifiers is right; a copied file with edited
   colours is not.
4. **Fill `validation_rules`.** They feed `validation.checks_performed` in real
   responses. A template with no validation rules cannot verify its own instances, which
   defeats the point.
5. **Fill `typical_ranges`.** This is what lets an agent pick plausible numbers without a
   round trip — cheap to write, disproportionately useful (`docs/WHY.md`).
6. **Record `known_failure_cases` as you hit them.** Symptom, cause, resolution, engine
   version. This is the section most likely to be skipped and the one that makes the
   library worth having in six months.
7. **Set `engine_compatibility`.** These are Experimental APIs (R-02); a template that
   worked on one build may not on the next.

## Provenance

`authored_by` is `human`, `agent`, or `promoted_from_asset`. When promoted, set
`promoted_from` to the source asset path. Knowing which templates an agent wrote versus a
human matters when one starts producing bad output.
