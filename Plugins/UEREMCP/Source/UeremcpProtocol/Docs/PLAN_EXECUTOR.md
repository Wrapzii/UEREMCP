# execute_plan interpreter

**Owner:** WS-05  
**Authority:** ADR-0008 and `schemas/batch/plan.schema.json`

`FUeremcpPlanExecutor` is the fail-closed protocol interpreter behind the
`UeremcpTemplates::SetExecutePlanDelegate` seam.

## Public integration API

- `RegisterAction` / `UnregisterAction` — domain-owned semantic handlers.
- `SetTransactionCallbacks` — integration-owned begin/commit/rollback.
- `ExecuteRequest` — accepts one normal `action: execute_plan` request and
  returns one consolidated response envelope.

Registration and execution snapshots are lock-protected. Handlers and
transaction callbacks run outside that lock.

## Execution contract

Before transaction begin, the interpreter:

1. parses the normal request envelope;
2. validates plan fields and operation shapes;
3. topologically sorts dependencies and rejects cycles/missing IDs;
4. confirms every semantic action has a handler;
5. rejects asset-state conditions until an evaluator contract is accepted; and
6. requires complete transaction callbacks for atomic plans.

Execution resolves `$ref` values from completed response envelopes, propagates
the parent options, and forces nested operation `timeout_ms` to zero. A
dependent operation never runs from a `partially_completed` result.

Compile/validation policy maps to each nested request:

- `per_operation` — every operation;
- `at_boundaries` — operations with dependents, plus the final operation;
- `at_end` — final operation;
- `never` — compile disabled (validation has no `never` option in the schema).

Required failure follows `on_failure`; atomic plans roll back when
`rollback_on_failure` is true or `on_failure` is `rollback_all`. Optional
failures do not stop independent work, but their dependents are skipped.

## Deliberate limitations

- No recursive `execute_plan` action registration.
- No asset-existence condition evaluator yet; such plans reject before begin.
- Nested long-running jobs are not composed. Internal operation timeout is
  forced inline so `$ref` never reads an unfinished result.
- Template `validation_rules` remain a separate post-step contract owned by
  WS-15/WS-11.
