# Proposal: WS-10 animation audit rows for WS-02

- **From:** WS-10
- **To:** WS-02 (`docs/audit/**`)
- **Date:** 2026-07-29
- **Status:** ready for merge by owner

## Why

`AGENTS.md` rule 2 — record Epic/REAgentTools equivalents before building. WS-10
researched RB-09; WS-02 owns `docs/audit/`.

## Suggested matrix updates (`epic-toolsets.md`)

Refine AnimationAssistant row and add adjacent rows:

| Plugin / surface | Tools | Disposition | Superseded by (UEREMCP) | Limitations | Tag |
|---|---|---|---|---|---|
| AnimationAssistant `ControlRigTools` | 44 | **preserve / compose** | `animation.*` / `control_rig.*` goal ops (Wave 3) | RigVM primitives; not envelope | `[VERIFIED: controlrig.py]` + runtime create |
| AnimationAssistant Sequencer* | 276 | **preserve / internalise** | bake path only | No AnimBP/montage/notify | `[VERIFIED: audit JSON 320 total]` |
| EditorToolset `SkeletalMeshTools` sockets/bones | ~15 anim-relevant | **preserve** | `animation.ensure_socket`, skeleton inspect | — | `[VERIFIED-RUNTIME]` |
| `UAnimationBlueprintLibrary` | n/a (not a toolset) | **internalise** | montage/notify/marker ops | Editor module | `[VERIFIED: AnimationBlueprintLibrary.h]` |
| AnimBP state machine authoring | **none in Epic toolsets** | **gap** | read-only inspect until proven | Schema spawners only | negative finding |

## REAgentTools (`reagenttools.md`)

| Toolset | Disposition | Notes |
|---|---|---|
| `REAnimWorkflowTools` | **become internal primitive** / replace with goal op | Pose timeline → bake → montage works; **notify plan is metadata-only** — do not preserve that as success |
| `RECharacterWorkflowTools` montage wiring | project-layer or compose | Ability contract with WS-09 |

## Do-not-rebuild additions

- Entire AnimationAssistant 320-tool surface
- `SkeletalMeshTools` socket/bone primitives
- Sequencer Control Rig keying (compose via REAnimWorkflow pattern)

## Real gap confirmation

- Goal-level montage + **real** AnimNotify authoring (library exists; no Epic toolset)
- AnimBP / state-machine structured inspect as ADR-0004 JSON (list_graphs works; DSL empty)
- AnimBP state-machine authoring remains a **documented non-goal** for Phase 4
