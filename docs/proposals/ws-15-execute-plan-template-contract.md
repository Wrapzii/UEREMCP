# WS-15 proposal: complete the execute_plan template contract

- **From:** WS-15 Templates
- **To:** WS-05 Protocol and WS-01 schema owner
- **Status:** executor and Templates binding landed; WS-01 accepted residual schema shapes
  (`docs/proposals/ws-01-template-validation-schema.md`)
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
- WS-08 and WS-07 register those two semantic handlers, and WS-03 registers the
  atomic begin/commit/rollback callbacks
  `[VERIFIED: UeremcpMaterialPlanHandlers.cpp:31-41;
  UeremcpNiagaraPlanHandlers.cpp:31-41;
  UeremcpPlanTransactionCoordinator.cpp:35-42]`.
- When the delegate is unbound, Instantiate returns an honest
  `partially_completed` / unverified response and states that no asset operation ran.
- Executable `validation_rules[].operation` steps are appended to the same plan.
  Templates preserves a validated domain status only when every exact
  `template.<template_id>.<rule_id>` appears in `validation.checks_performed`;
  missing evidence and check-only rules downgrade to `partially_completed`.

Promotion contract details live in `ws-15-promotion-gate-handoffs.md`.

WS-01 accepted the WS-05 `1ef125d` residual contract in `4eedd86`. WS-15 now
materializes deterministic operation replacements, RFC 7396 specification merges,
appended construction/validation operations, and template validation operations.
Declaration-only modifiers and check-only rules remain deliberately non-executable.

`promote_to_template` additionally requires a domain-neutral complete-state retrieval
and a schema-valid way to express the resulting construction plan. No such dispatcher
contract exists in WS-15-owned paths.

## Requested owned changes

### WS-05

1. **Landed:** fail-closed `FUeremcpPlanExecutor` interpreter.
2. **Landed in WS-15:** startup delegate binding and shutdown clearing.
3. **Residual:** aggregate nested operation validation evidence in execute_plan.
   Templates already compares the consolidated result against exact expected IDs.

### WS-03 / domain workstreams

**Landed:** `create_vfx_material`, `create_niagara_effect`, and complete
begin/commit/rollback callbacks are registered on orch. Exact lifecycle, adapter,
evidence, and fail-closed behavior: `ws-15-plan-handler-registration.md`.

### WS-01

**Accepted:** executable `modifier_definitions` and `validation_rules[].operation`
shapes from WS-05 `1ef125d` are now in
`schemas/template-library/template.schema.json`
(`docs/proposals/ws-01-template-validation-schema.md`). Names and descriptive
`check` strings alone remain non-executable.

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
- Declared modifiers without executable definitions, duplicate names, bucket
  mismatches, unknown operation IDs, duplicate materialized operation IDs, and
  missing dependencies fail before delegation.
- A delegated domain result is downgraded to `partially_completed` when template
  validation evidence is absent, with each exact rule evidence ID listed under
  `validation.checks_skipped`. Check-only legacy rules always take this path.

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
