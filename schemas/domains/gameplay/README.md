# Gameplay domain schemas

Owner: WS-09.

`create_spell.schema.json` is the RE-native semantic contract for POC D. It maps
to one `FREAbilityDef` row consumed by `CastAbility` / `AuthorityCastAbility`; it
does not create disconnected Epic GAS assets
`[VERIFIED: REAbilityTypes.h:85-247; REPlayerVisualCombatComponent.cpp:3912-3984]`.

The envelope target is the DataTable package path. `row_name` is both the row key
and `AbilityId`; repeated requests never auto-suffix either identity (ADR-0006).
POC paths are restricted to `/Game/__UeremcpTests/` until production-table policy
is explicitly approved.

Presentation fields are soft references to WS-07/WS-08 outputs. The schema does
not recreate Niagara or material primitives. The networking block declares the
existing RE Pattern B contract; it does not generate RPCs or claim multi-client
runtime proof.

Current implementation boundary:

- deterministic specification-to-row planning and static validation: implemented;
- DataTable mutation/re-read/save: gated by the ADR-0010 single-mutator queue;
- agent-facing registration: gated by adding `UeremcpGameplay` to
  `UEREMCP.uplugin` (WS-03-owned path);
- listen-server replication proof: WS-11/RB-14.
