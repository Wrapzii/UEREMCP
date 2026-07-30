# Proposal: POC D must target RE magecraft, not textbook GAS

- **From:** WS-09
- **To:** WS-01 (POC_ACCEPTANCE / example schemas), WS-05 (batch actions)
- **Date:** 2026-07-29
- **Status:** proposed
- **Evidence:** `docs/research/RB-12-gas-and-gameplay.md`

## Problem

`docs/POC_ACCEPTANCE.md` POC D and `schemas/examples/batch-fireball-ability.json`
assume `create_gameplay_ability` + `create_gameplay_effect` (Epic GAS shapes).

RE does not use Epic GAS. Abilities are `FREAbilityDef` rows in `DT_Abilities`,
executed by `CastAbility` / `AuthorityCastAbility`
`[VERIFIED: REAbilityTypes.h]`, `[VERIFIED: REPlayerVisualCombatComponent.cpp]`.

Implementing textbook GAS assets would satisfy a schema while producing content
the project cannot cast — a false POC.

## Ask

1. Reinterpret POC D criteria D1–D8 as:
   - one `execute_plan` upserts an ability **row** (+ VFX assets + optional
     `URESpellVFXDefinition`) under `/Game/__UeremcpTests/`
   - "gameplay tags assigned" → RE `Element` / `EffectTag` / `ImpactStatus` fields
     (or optional cue tags only if explicitly required)
   - "replication validated" → static Pattern B checklist + optional
     `pie_cast_and_capture`; multi-client net proof via RB-14
2. Replace example actions:
   - `upsert_ability_row` / `create_spell` instead of `create_gameplay_ability`
   - drop or demote `create_gameplay_effect` for RE-native mode
3. Keep elemental variants as **one parameterized** `create_spell`, not per-element
   primitives.

## Non-ask

WS-09 will not edit `schemas/**` or `docs/POC_ACCEPTANCE.md` (WS-01/WS-05 owned).
Phase 4 implementation waits on acceptance.

## Response (WS-01)

**Accepted — 2026-07-29.**

- `docs/POC_ACCEPTANCE.md` POC D rewritten for RE magecraft (`create_spell` /
  `upsert_ability_row`, Pattern B, no textbook GA/GE).
- `schemas/examples/batch-fireball-ability.json` updated (test paths under
  `/Game/__UeremcpTests/`). Embedded `plan.schema.json` example left to WS-05
  via `ws-01-plan-example-create-spell.md`.
- Elemental variants stay one parameterized family (ADR-0008).

Domain schemas under `schemas/domains/gameplay/**` remain WS-09 Wave 3
implementation. Do not start until Phase 1 exit.
