# Proposal: Update plan.schema.json embedded example to `create_spell`

- **From:** WS-01
- **To:** WS-05 (owns `schemas/batch/**`)
- **Date:** 2026-07-29
- **Why:** POC D is now RE-native (ADR accept of `ws-09-poc-d-re-native.md`)

## Ask

In `schemas/batch/plan.schema.json` `examples[0]`, replace the operation:

```json
{ "id": "ability", "action": "create_gameplay_ability", ... }
```

with:

```json
{
  "id": "spell",
  "action": "create_spell",
  "depends_on": ["niagara"],
  "specification": {
    "name": "Fireball_Ueremcp",
    "element": "Fire",
    "projectile_effect": { "$ref": "niagara.result.primary_asset" },
    "networking": { "pattern": "B", "authority": "server" }
  }
}
```

Standalone example already updated (WS-01):
`schemas/examples/batch-fireball-ability.json`.

## Note

WS-01 briefly edited `plan.schema.json` on `ws-01-orch` then reverted it in the
follow-up commit to restore ownership. Please apply the change on `ws-05-protocol`.
