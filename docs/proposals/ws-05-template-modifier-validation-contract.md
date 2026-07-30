# WS-05 proposal: executable template modifiers and validation rules

**From:** WS-05 Protocol  
**To:** WS-01 schema owner, WS-15 Templates, domain workstreams  
**Status:** accepted by WS-01 into `schemas/template-library/template.schema.json`
(see `docs/proposals/ws-01-template-validation-schema.md`)
**Closes:** the two residual ADR-0008 contract gaps recorded in
`ws-15-execute-plan-template-contract.md`

## Problem

The frozen template schema currently provides:

- `supported_modifiers`: names only, with no deterministic effect; and
- `validation_rules[].check`: a descriptive string, with no registered semantic
  action that can execute it.

Names and expression-like strings are not executable contracts. Reporting either as
applied or checked would be false validation. The existing fail-closed downgrade to
`partially_completed` must remain until the contracts below land.

## Decision constraints

1. Keep one interpreter: every generated step remains a normal
   `schemas/batch/plan.schema.json#/$defs/operation`.
2. Do not add arbitrary expression or Python execution.
3. Materialization must be deterministic and must validate the complete resulting
   plan before `FUeremcpPlanExecutor` begins a transaction.
4. Unknown actions, operation IDs, modifier names, or validation handlers fail before
   mutation.
5. Internal execution remains one MCP round trip.

## Proposed modifier definition

WS-01 should add `modifier_definitions` to
`schemas/template-library/template.schema.json`. It is an object keyed by the names
accepted in `instantiate_template.specification.modifiers`.

Each definition has:

```json
{
  "bucket": "replace | adjust | add | preserve",
  "replace_operations": {
    "existing_operation_id": {
      "id": "existing_operation_id",
      "action": "create_vfx_material",
      "specification": {}
    }
  },
  "merge_specifications": {
    "existing_operation_id": {
      "feature": { "enabled": true }
    }
  },
  "append_operations": [
    {
      "id": "extra_operation",
      "action": "create_niagara_effect",
      "depends_on": ["existing_operation_id"],
      "specification": {}
    }
  ],
  "validation_operations": []
}
```

All effect fields are optional, but at least one must be non-empty.

### Deterministic application

1. Reject duplicate modifier names across request buckets.
2. Resolve every name in `modifier_definitions`; the definition's `bucket` must match
   the request bucket containing it.
3. Apply buckets in fixed order: `replace`, `adjust`, `add`, `preserve`.
4. Within a bucket, apply definitions in request-array order.
5. `replace_operations` replaces one complete operation and must preserve its key as
   the replacement `id`.
6. `merge_specifications` applies RFC 7396 JSON Merge Patch semantics only to the
   named operation's `specification`. It cannot alter `id`, `action`, dependencies,
   target, mode, or transaction policy.
7. `append_operations` appends complete schema-valid operations. IDs must remain
   globally unique.
8. Append `validation_operations` after construction effects.
9. Validate the final materialized plan against `plan.schema.json`, then run the
   existing dependency preflight. Any failure rejects before delegation.

`supported_modifiers` may remain temporarily as a search/display index, but a name is
executable only when an equal key exists in `modifier_definitions`. WS-15 must reject
declaration-only names.

## Proposed executable validation rule

WS-01 should extend each template `validation_rules` item with a required
`operation` for executable rules:

```json
{
  "rule_id": "six_emitters",
  "severity": "error",
  "message": "Elemental projectile must compose all six emitters.",
  "operation": {
    "id": "validate_six_emitters",
    "action": "validate_niagara_system",
    "depends_on": ["projectile_fx"],
    "specification": {
      "expected_emitter_count": 6
    }
  }
}
```

The existing `check` string becomes optional descriptive legacy metadata. A rule with
`check` but no `operation` is not executable and continues to force
`partially_completed`.

WS-15 materializes each validation `operation` into the same plan after construction
operations. It does not invoke a second validator or evaluate `check`.

### Validation evidence

For a rule to count as performed:

1. its semantic action handler is registered before transaction begin;
2. the operation executes after all declared dependencies;
3. the handler returns a successful status; and
4. its response contains
   `validation.checks_performed: ["template.<template_id>.<rule_id>"]`.

The consolidated `execute_plan` response must union `checks_performed`,
`checks_skipped`, warnings, and errors from successful operation responses while
preserving operation order and removing exact duplicate check IDs.

Templates compares the expected executable rule IDs with that union:

- every required ID present and no error-severity rule failed: preserve the domain
  validated status;
- warning-severity rule failed: `created_with_warnings`;
- error-severity rule failed: `failed_validation` or `rolled_back`, according to the
  plan transaction policy;
- handler missing, operation skipped, evidence absent, or legacy-only `check`:
  `partially_completed` with the exact rule ID in `checks_skipped`.

## Ownership handoff

- **WS-01:** amend the frozen template schema.
- **WS-15:** materialize definitions/rules and compare expected validation evidence.
- **Domain WS:** register semantic validation handlers; no generic expression engine.
- **WS-05:** after schema acceptance, aggregate validation evidence in the existing
  execute-plan response consolidation and add protocol tests.

Until all owned pieces land, current behavior remains correct: modifiers without
effects fail, and unexecuted template rules downgrade otherwise successful domain
results to `partially_completed`.
