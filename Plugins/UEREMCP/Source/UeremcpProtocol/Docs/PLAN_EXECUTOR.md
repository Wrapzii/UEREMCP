# execute_plan interpreter

**Owner:** WS-05  
**Authority:** ADR-0008 and `schemas/batch/plan.schema.json`

`FUeremcpPlanExecutor` is the fail-closed protocol interpreter behind both:

1. `UeremcpTemplates::SetExecutePlanDelegate` (template instantiation), and
2. `FUeremcpPlanActions::ExecutePlan` (agent-facing string adapter for Core's
   forthcoming `AICallable` wrapper — see `docs/proposals/ws-05-execute-plan-aicallable.md`).

## Public integration API

- `RegisterAction` / `UnregisterAction` — domain-owned semantic handlers.
- `SetTransactionCallbacks` — integration-owned begin/commit/rollback.
- `ExecuteRequest` — accepts one normal `action: execute_plan` request and
  returns one consolidated response envelope.
- `FUeremcpPlanActions::ExecutePlan` — same request → response **string**, plus
  idempotency replay and ADR-0009 timeout/partial handling for the initiating call.

Registration and execution snapshots are lock-protected. Handlers and
transaction callbacks run outside that lock.

## Template bind

WS-15 binds and clears the delegate in `UeremcpTemplatesModule`; that binding is
present on this synced branch. Protocol cannot depend on Templates. Domain
handler and transaction registration remain separate integration requirements.

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
dependent operation never runs from a non-success status (including optional
failures and `partially_completed` skips).

Compile/validation policy maps to each nested request:

- `per_operation` — every operation;
- `at_boundaries` — operations with dependents, plus the final operation;
- `at_end` — final operation;
- `never` — compile disabled (validation has no `never` option in the schema).

Required failure follows `on_failure`:

- `stop` — skip remaining operations;
- `continue_independent` — keep running ops whose dependencies succeeded;
- `rollback_all` — treat as required failure for rollback accounting.

Atomic plans roll back when `rollback_on_failure` is true or `on_failure` is
`rollback_all`. Optional failures do not stop independent work, but their
dependents are skipped. `operation_status` conditions may skip an op with
`no_change_required` without failing the plan.

## Domain registration

See `HANDLER_REGISTRATION.md` for the exact `RegisterAction` / shutdown pattern
and the minimum action list for current templates.

## Offline mirror

Python `Tests/py/ueremcp_protocol/plan_executor.py` mirrors this interpreter for
outside-editor regression. Run via `Tests/py/run_tests.py`
(`test_plan_executor.py`). Python green is not C++ Automation parity.

## Deliberate limitations

- No recursive `execute_plan` action registration.
- No asset-existence condition evaluator yet; such plans reject before begin.
- Nested long-running jobs are not composed. Internal operation timeout is
  forced inline so `$ref` never reads an unfinished result.
- Template `validation_rules` remain a separate post-step contract owned by
  WS-15/WS-11.
- Production `timeout_ms` **async scheduler** for long single operations is
  Core/Transport (`docs/proposals/ws-05-timeout-dispatcher.md`). The Protocol
  adapter exposes the ADR-0009 envelope behaviour (inline vs
  `partially_completed` + job) and a deterministic force-timeout test probe;
  it does not replace Core's production dispatcher.
