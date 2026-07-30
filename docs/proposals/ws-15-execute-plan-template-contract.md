# WS-15 proposal: complete the execute_plan template contract

- **From:** WS-15 Templates
- **To:** WS-05 Protocol and WS-01 schema owner
- **Status:** executor and Templates binding landed; residual contracts proposed
- **Blocks:** fully validated `instantiate_template`; executable named modifiers;
  `promote_to_template`

## Evidence from the synced tree

ADR-0008 requires `instantiate_template` to materialise a template and delegate to
WS-05 `execute_plan`. WS-15 now exposes `UeremcpTemplates::SetExecutePlanDelegate`
and submits one complete request envelope whose action is `execute_plan` and whose
specification conforms to `schemas/batch/plan.schema.json`.

As of the current Templates tree:

- `FUeremcpPlanExecutor` landed from WS-05 (`cc79f22`, integrated by orch as
  `1659633`), and `FUeremcpTemplatesModule::StartupModule` binds
  `UeremcpTemplates::SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest)`.
  Shutdown clears the delegate before destroying the service.
- Element presets under `templates/elements/` inject material overrides and Niagara
  parameters while materializing `niagara.projectile.elemental.v1`.
- Materialized operations validate against `create_vfx_material` /
  `create_niagara_effect` schemas offline.
- When the delegate is unbound, Instantiate returns an honest
  `partially_completed` / unverified response and states that no asset operation ran.
- When the delegate returns a domain `*_validated` result but template
  `validation_rules` could not run, Templates downgrades to `partially_completed`
  and lists `template.validation_rules` under `validation.checks_skipped`.

Promotion contract details live in `ws-15-promotion-gate-handoffs.md`.

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

1. **Landed:** fail-closed `FUeremcpPlanExecutor` interpreter.
2. **Landed in WS-15:** startup delegate binding and shutdown clearing.
3. **Residual:** define how template validation post-steps are represented and returned in
   `validation.checks_performed`, without introducing a second interpreter.

### WS-03 / domain workstreams

Register `create_vfx_material` and `create_niagara_effect` semantic handlers plus
complete begin/commit/rollback transaction callbacks. Until those capabilities are
registered, the bound executor rejects before mutation; binding alone does not make
elemental instantiation complete.

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

## Promotion scaffold (WS-15)

`promote_to_template` now validates the owned specification, resolves an existing
`base_template_id`, derives or validates a versioned template id, and previews the
agent quarantine path. It performs zero asset or filesystem mutations and reports
`partially_completed`.

The response lists these unrun contract gates under `validation.checks_skipped`:

- `template.promotion.complete_state_retrieval`
- `template.promotion.reproduction_plan_synthesis`
- `template.promotion.schema_validation`
- `template.promotion.security_write_gate`
- `template.promotion.quarantine_write`

Both `options.dry_run: false` and `quarantine: false` remain non-mutating requests
until those gates are implemented. This is deliberate fail-closed behavior, not a
successful promotion.
