# Proposal: Animation ↔ ability contract (WS-10 → WS-09)

- **From:** WS-10
- **To:** WS-09
- **Date:** 2026-07-29
- **Related:** RB-09, RB-12

## Goal

One `create_spell` / ability request can wire animation without a second round-trip
(`docs/WHY.md` cost model).

## Contract fields (WS-10 produces / validates)

| Field | Required | Notes |
|---|---|---|
| `montage.path` | yes | AnimMontage under allowed roots |
| `montage.slot` | no (default `DefaultSlot` / project convention) | |
| `notifies[]` | yes for combat/VFX timing | **Real** `UAnimNotify` / `UAnimNotifyState` via `UAnimationBlueprintLibrary`, not metadata tags |
| `notifies[].time` | yes | seconds |
| `notifies[].class` | yes | Soft class path |
| `notifies[].socket` | when VFX | Must exist on mesh; WS-10 can `ensure_socket` |
| `root_motion` | no | default false for ability casts unless requested |
| `skeleton` / `mesh` | for validation | Resolve sockets against mesh, not Skeleton asset misnamed `SK_*` |

## WS-09 owns

- Ability / GA asset creation and montage soft refs
- Gameplay cue / tag binding triggered by notify class choice
- Replication policy for ability activation (WS-10 validates montage flags only)

## Explicit non-promises

- AnimBP state-machine changes for abilities — **out of scope**
- Control Rig graph authoring for spell cast — use montage path
- REAgentTools `RENotifyPlan:` tags as notify substitutes — **rejected**

## Verification

Status `created_and_validated` only when montage re-read shows notify events at
requested times with expected classes, and socket exists if referenced.

## Response (WS-01)

**Accepted with RE carve-out.** Real `UAnimNotify*` (not REAgentTools metadata
tags) is required whenever montage wiring is in scope. For **RE POC D**,
`FREAbilityDef` has no cast-montage field today (`ws-09-cue-vfx-contract.md`) —
montage remains **optional** for D1–D8; Niagara soft paths are the primary
presentation seam. When a target system has montage soft refs, this contract
applies and `montage.path` + real notifies are required for `*_validated`.
