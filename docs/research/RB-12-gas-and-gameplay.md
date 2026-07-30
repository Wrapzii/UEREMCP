# RB-12: Gameplay Ability System and player-system authoring

- **Owner:** WS-09
- **Status:** not_started
- **Blocks:** POC D (batched gameplay ability)
- **Priority:** medium-high

## Framing

POC D exists to prove the architecture is not Niagara-only. Its value is in the
**multi-asset batch**: one request producing an ability, an effect, tags, a projectile
actor, VFX cues, and replication config, all validated together.

Epic ships `GASToolsets` and `GameplayTagsToolset` `[VERIFIED: $TS listing]` — neither
is listed as enabled in `RE.uproject` `[VERIFIED]`, so check load state first.
REAgentTools has no GAS graph authoring but notes `DT_Abilities` and `CastAbility`
exist in the project `[UNVERIFIED — from $RAT/Docs/CAPABILITY_MATRIX.md]`.

**Understand the RE project's existing gameplay architecture before designing
anything.** `create_spell` must produce assets that fit how RE already works, not a
generic GAS textbook shape. `$PROJ/AGENT_SYNC.md` and `$PROJ/Source/RE` are the sources.

## Questions

1. Is GAS actually in use in RE, and how? `UGameplayAbility` subclasses in C++ or
   Blueprint? Is `DT_Abilities` a data-driven layer that partly bypasses GAS?
   **Answer this first — it determines everything else.**
2. Can `UGameplayAbility` Blueprint subclasses be created and configured
   programmatically — tags, costs, cooldowns, instancing policy, net execution policy?
3. `UGameplayEffect`: creatable with modifiers, executions, duration, stacking,
   granted tags?
4. Attribute sets — inspectable, and can attributes be referenced safely in generated
   effects?
5. Gameplay tags: can tags be added to the project's tag tables programmatically, and
   how do we avoid tag-table churn conflicts between concurrent agents? (Concrete
   ADR-0006 concurrency case — flag to WS-01.)
6. Ability tasks, targeting, and montage integration from a generated ability graph —
   how much is Blueprint-graph authoring (WS-06's machinery) versus class defaults?
7. Gameplay cues: how are Niagara and audio bound? This is the seam to WS-07 and the
   reason POC D matters — coordinate the contract.
8. **Replication:** how do we *validate* that a generated ability is correctly
   replicated? Master prompt §7 asks for unsafe-replication detection. What is
   statically checkable — net execution policy vs authority, replicated variables,
   RPC direction — versus what needs a PIE test?
9. Can a networked ability be smoke-tested — PIE with a listen server, activate,
   observe? What does that cost, and can it be automated (RB-14)?
10. Projectile actors: creatable and configurable, including collision, movement
    component, and replication, from a spec?
11. What is the minimum viable `create_spell` specification that covers the master
    prompt's example — name, element, delivery, speed, damage radius, visual style, cast
    animation, networking — and what does it decompose into as a batch plan?

## Deliverables

- [ ] A written description of RE's actual ability architecture, so generated content
      fits it
- [ ] `schemas/domains/gameplay/` specification schemas, including `create_spell`
- [ ] POC D: one batch request producing an ability + effect + tags + cue, validated
- [ ] A replication validation checklist: statically checkable vs requires-PIE
- [ ] The cue↔VFX contract agreed with WS-07, and the montage contract with WS-10
- [ ] The gameplay-tag concurrency hazard (q5) raised to WS-01
