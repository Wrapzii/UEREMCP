# WS-15 proposal: unblock promote_to_template contract gates

- **From:** WS-15 Templates
- **To:** WS-05 Protocol, WS-12 Security, domain workstreams (WS-06/07/08)
- **Status:** proposed
- **Blocks:** mutating `promote_to_template`; honest `*_validated` promotion
- **Related:** `ws-15-execute-plan-template-contract.md`

## Current WS-15 behavior (already landed)

`promote_to_template` validates ownership-owned specification fields, resolves an
existing `base_template_id`, derives or validates a versioned template id, and
previews `/Game/__UeremcpTemplates/agent/<template_id>`. It performs **zero**
asset inspection and **zero** filesystem or Content writes. Status is always
`partially_completed` (or `failed_validation` / `rejected` on contract failure).

Skipped gates reported under `validation.checks_skipped`:

| Gate | Owner | What must exist before WS-15 may claim progress |
|---|---|---|
| `template.promotion.complete_state_retrieval` | WS-05 + domains | Domain-neutral dispatcher that returns complete structured state for a soft path (Niagara/material/Blueprint) |
| `template.promotion.reproduction_plan_synthesis` | WS-15 (after retrieval) | Diff source state against optional base template → schema-valid `construction_plan` |
| `template.promotion.schema_validation` | WS-15 | Generated JSON validates against `template.schema.json` before any write |
| `template.promotion.security_write_gate` | WS-12 | Shared path/tier/`dry_run` policy authorizes writes under `/Game/__UeremcpTemplates/agent/` |
| `template.promotion.quarantine_write` | WS-15 (after security) | Persist quarantine JSON only after the gates above pass |

## Requested owned changes

### WS-05 — complete-state retrieval seam

Expose a registered entry point Templates can call without importing domain
modules. Either:

1. a C++ delegate parallel to `UeremcpTemplates::SetExecutePlanDelegate`, or
2. an internal action routed through the same executor registry as `execute_plan`.

Minimum request shape Templates will send:

```json
{
  "protocol_version": "1.0",
  "action": "retrieve_complete_state",
  "target": { "asset_path": "/Game/VFX/Spells/NS_Fireball" },
  "specification": {
    "purpose": "template_promotion",
    "include_graph": true,
    "include_dependencies": true
  }
}
```

Minimum success payload Templates need:

```json
{
  "status": "no_change_required",
  "result": {
    "primary_asset": "/Game/VFX/Spells/NS_Fireball",
    "asset_class": "NiagaraSystem",
    "domain": "niagara",
    "complete_state": {}
  },
  "validation": {
    "reread_after_write": null,
    "checks_performed": ["domain.complete_state_read"]
  },
  "metrics": { "mcp_round_trips": 0, "internal_operations": 1 }
}
```

If retrieval is unsupported for the asset class, return `partially_completed` with
`checks_skipped` naming the missing domain reader — never invent state.

Also keep binding `execute_plan` to `UeremcpTemplates::SetExecutePlanDelegate`
(see `ws-15-execute-plan-template-contract.md`).

### WS-12 — quarantine write authorization

1. Authorize `/Game/__UeremcpTemplates/agent/**` as the only runtime-writable
   template root for agent-authored promotions.
2. Force `dry_run: true` for promotion writes unless the shared write policy
   explicitly permits `dry_run: false`.
3. Reject traversal and non-`/Game/` sources at the shared gate (Templates also
   fail-closes locally today).

Suggested gate inputs Templates will supply:

```json
{
  "action": "promote_to_template",
  "mode": "create",
  "target_path": "/Game/__UeremcpTemplates/agent/niagara.projectile.ns_fireball.v1",
  "source_asset": "/Game/VFX/Spells/NS_Fireball",
  "dry_run": true,
  "quarantine": true
}
```

Suggested deny reasons Templates will surface unchanged in `summary` /
`capability_notes`: path rejected, tier insufficient, foreign sandbox active,
destructive override required.

### Domain workstreams (WS-06 / WS-07 / WS-08)

Provide complete-state inspectors suitable for promotion diffs. Promotion with a
`base_template_id` is the intended path; promotion without a base produces a
copy-shaped template and must not claim POC C7 pattern status.

## WS-15 fail-closed until then

- `options.dry_run: false` and `quarantine: false` remain non-mutating.
- No `changes` / `result.created_assets` are emitted by the scaffold.
- No `*_validated` status is returned for promotion.
- Offline tests assert the exact five-gate skipped set and zero filesystem writes.
