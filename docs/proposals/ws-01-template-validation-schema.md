# WS-01 decision: executable template modifiers and validation

- **From:** WS-01 schema owner
- **To:** WS-05 Protocol, WS-15 Templates, domain workstreams
- **Date:** 2026-07-30
- **Status:** accepted
- **Responds to:** `docs/proposals/ws-05-template-modifier-validation-contract.md` (`1ef125d`)
- **Closes residual asked in:** `docs/proposals/ws-15-execute-plan-template-contract.md`

## Decision

Accept the WS-05 residual contract without inventing a second interpreter or
expression language.

Amended `schemas/template-library/template.schema.json`:

1. **`modifier_definitions`** — optional object keyed by modifier name. Each
   definition requires `bucket` (`replace|adjust|add|preserve`) and at least one
   of `replace_operations`, `merge_specifications`, `append_operations`, or
   `validation_operations`. Effect payloads reuse
   `schemas/batch/plan.schema.json#/$defs/operation` (or specification-only merge
   patches).
2. **`validation_rules[].operation`** — optional normal plan operation. A rule is
   executable only when `operation` is present. Legacy `check` strings remain
   allowed as descriptive metadata and stay non-executable.
3. **`supported_modifiers`** — retained as a search/display index. A name is
   executable only when an equal key exists in `modifier_definitions`.

## Compatibility

Existing seeded templates that ship `validation_rules` with `check` only remain
schema-valid. Instantiation must continue to fail closed /
`partially_completed` for declaration-only modifiers and check-only rules until
WS-15 materializes definitions and domains register validation handlers.

## Ownership after acceptance

| Owner | Next work |
|---|---|
| WS-15 | Materialize `modifier_definitions` / validation `operation` steps; keep fail-closed for check-only rules |
| Domain WS | Register semantic validation actions used by template `operation.action` |
| WS-05 | Aggregate `template.<template_id>.<rule_id>` evidence into execute_plan consolidation |

No ADR is Accepted or rewritten by this decision. ADR-0008 remains the binding
substrate; this only fills the frozen template schema gaps that blocked honest
`*_validated` statuses.
