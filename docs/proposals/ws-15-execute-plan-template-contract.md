# WS-15 proposal: complete the execute_plan template contract

- **From:** WS-15 Templates
- **To:** WS-05 Protocol and WS-01 schema owner
- **Status:** proposed
- **Blocks:** fully validated `instantiate_template`; executable named modifiers;
  `promote_to_template`

## Evidence from the synced tree

ADR-0008 requires `instantiate_template` to materialise a template and delegate to
WS-05 `execute_plan`. The current tree contains the accepted batch schema and protocol
helpers, but no `execute_plan` executor or action registration under
`Plugins/UEREMCP/Source/UeremcpProtocol/**`.

WS-15 now exposes `UeremcpTemplates::SetExecutePlanDelegate` and submits one complete
request envelope whose action is `execute_plan` and whose specification conforms to
`schemas/batch/plan.schema.json`. Until WS-05 binds its executor, the action reports
`partially_completed` and explicitly states that no asset operation ran.

Two frozen-schema gaps prevent an honest validated status:

1. `template.schema.json` names `supported_modifiers` but has no field that defines
   the executable delta for a named modifier. Treating a name as applied without a
   delta silently does nothing.
2. `template.schema.json` defines `validation_rules`, but `plan.schema.json` has no
   post-validation rule collection or registered validation operation contract.
   WS-15 cannot append those rules without producing a plan that fails the accepted
   batch schema.

`promote_to_template` additionally requires a domain-neutral complete-state retrieval
and a schema-valid way to express the resulting construction plan. No such dispatcher
contract exists in WS-15-owned paths.

## Requested owned changes

### WS-05

1. Implement/register the accepted `execute_plan` interpreter.
2. Bind it to `UeremcpTemplates::SetExecutePlanDelegate` during module startup and
   clear the binding during shutdown.
3. Define how template validation post-steps are represented and returned in
   `validation.checks_performed`, without introducing a second interpreter.

### WS-01

Extend the frozen template schema with executable named modifier definitions. A
minimal shape should map each supported modifier name to a schema-valid plan delta or
additional operation list; names alone are not executable.

## Required behavior after integration

- One MCP call enters `instantiate_template`; internal delegation does not increment
  `metrics.mcp_round_trips`.
- The complete execute_plan `result`, `validation`, `changes`, `diagnostics`,
  `revision`, `rollback`, and metrics survive unchanged.
- `understood.template_used` and the resolved target are added by Templates.
- A `*_validated` status is returned only after both domain validation and every
  template `validation_rule` actually run.
- Elemental variants remain one template family selected by `inputs.element`;
  no per-element actions or template forks are introduced.

## WS-15 fail-closed behavior meanwhile

- Missing, mistyped, out-of-range, or invalid-enum inputs fail before delegation.
- Unknown modifiers fail before delegation.
- Declared modifiers without executable deltas also fail, rather than reporting a
  no-op as applied.
- A delegated domain result is downgraded to `partially_completed` when template
  validation rules could not run, with `template.validation_rules` listed under
  `validation.checks_skipped`.
