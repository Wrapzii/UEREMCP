# Template authoring

**Owner:** WS-13 (guide). Template documents and runtime: **WS-15**.

Do not duplicate the full authoring rules here. Canonical text:

- [`templates/README.md`](../../templates/README.md)
- ADR-0008 — JSON + `UeremcpTemplates` + internal `execute_plan` (not `UAgentSkill`)
- Schemas: [`schemas/template-library/template.schema.json`](../../schemas/template-library/template.schema.json),
  [`schemas/domains/templates/`](../../schemas/domains/templates/)

## Agent-facing operations

| Action | Tool | Notes |
|---|---|---|
| `search_templates` | `SearchTemplates` | Filter by query / domain / element |
| `instantiate_template` | `InstantiateTemplate` | Materializes plan → WS-05 interpreter |
| `promote_to_template` | `PromoteToTemplate` | Preview / quarantine until gates bind |

Worked envelopes: [`capability-reference.md`](capability-reference.md).

## Author checklist (short)

1. `template_id` matches filename pattern `<domain>.<category>.<name>.v<n>`
2. `construction_plan` operations conform to batch operation shape
3. `python tools/validate_schemas.py` green
4. Fill `validation_rules`, `typical_ranges`, and `known_failure_cases` as you learn them
5. Prefer `inherits_from` / modifiers over copy-paste elemental variants

POC C (variation + C7 third instantiate) is **not** claimed on this tip.
