# Proposal: Animation domain specification schemas (deferred create)

- **From:** WS-10
- **To:** WS-01 / WS-05 (schemas ownership); WS-10 will create under
  `schemas/domains/animation/` only when Wave 3 implementation is authorized
- **Date:** 2026-07-29
- **Related:** RB-09, ADR-0003/0004

## Intent

Extend **`specification` only** (never envelope/graph). Proposed files when
implementation starts:

```
schemas/domains/animation/
  inspect_montage.specification.schema.json
  ensure_montage.specification.schema.json
  inspect_sequence.specification.schema.json
  ensure_socket.specification.schema.json
  read_anim_bp.specification.schema.json
  graph-ext.schema.json          # extensions.anim + extensions.control_rig
```

## `extensions.anim` (AnimBP / SM)

```json
{
  "skeleton": "/Game/.../SK_Mannequin",
  "preview_mesh": "/Game/.../SKM_Manny_Simple",
  "state_machines": [
    {
      "name": "Locomotion",
      "entry_state": "Idle",
      "states": [{ "name": "Idle", "bound_graph_id": "..." }],
      "transitions": [{ "from": "Idle", "to": "Walk / Run", "rule_graph_id": "..." }]
    }
  ]
}
```

Always pair with `fidelity.round_trip_supported: false` until proven.

## `extensions.control_rig`

```json
{
  "hierarchy_summary": { "bone_count": 160, "control_count": 0 },
  "unit_struct_paths": ["/Script/ControlRig.RigUnit_BeginExecution"],
  "contained_graph_ids": []
}
```

## Actions **not** to schema yet

- `submit_anim_state_machine` / SM replace
- Sequencer primitive mirrors
- IK retarget goal ops (defer)

## capability_notes

See RB-09 — ship on every animation response.
