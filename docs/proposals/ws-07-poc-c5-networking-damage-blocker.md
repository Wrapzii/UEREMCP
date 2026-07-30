# WS-07: POC C5 networking and damage blocker

- **Owner:** WS-07
- **Date:** 2026-07-30
- **Status:** blocked
- **Criterion:** POC C C5 — “Networking and damage behaviour from the source are
  preserved and verified unchanged.”
- **Decision:** Keep C5 **FAIL**. Do not infer a gameplay contract from Niagara
  structure or from the `preserve_networking` modifier name.

## Audit result

The current POC C source is a Niagara asset only:
`/Game/__UeremcpPoc/NS_POCB_Fireball`. The variation request passes that path as
`specification.base_system.asset_path`; `FUeremcpNiagaraCreate` duplicates the
system and preserves emitter names, while the template records
`source_networking_and_damage_unchanged` as an unverified declarative string. No
source actor, ability row, damage definition, or networking object is identified
or compared `[VERIFIED:
Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraCreate.cpp:459-463,660-664]`
`[VERIFIED:
templates/niagara/niagara.projectile.elemental.v1.json:92-105,140-164]`.

The real project surfaces audited below do not close that gap:

1. The production Niagara prior art
   `/Game/VFX/Spells/Firebolt/Systems/NS_FB_Projectile` exposes 18 `User.*`
   variables, all named for color, scale, shape, or velocity. It exposes no
   networking or damage parameter `[VERIFIED-RUNTIME: NiagaraToolset_System.
   GetSystemSummary on NS_FB_Projectile, 2026-07-30]`. Its metadata map is empty
   `[VERIFIED-RUNTIME: AssetTools.get_metadata_tags on NS_FB_Projectile,
   2026-07-30]`.
2. The production actor
   `/Game/VFX/Spells/Firebolt/Blueprints/BP_FB_Firebolt` has a real networking
   configuration: its CDO reports `bReplicates=true` and
   `bReplicateMovement=true`; its asset registry tag reports
   `NumReplicatedProperties=0` `[VERIFIED-RUNTIME:
   ObjectTools.get_properties and AssetTools.get_asset_tags on BP_FB_Firebolt,
   2026-07-30]`. Its event graph spawns impact/aftermath Niagara and destroys the
   actor on collision, but contains no damage, authority, server, multicast, or
   replication node `[VERIFIED-RUNTIME: BlueprintTools.read_graph_dsl on
   BP_FB_Firebolt EventGraph, 2026-07-30]`.
3. `BP_ThirdPersonCharacter.CastFirebolt` directly spawns
   `BP_FB_Firebolt`; the function contains no authority or damage operation
   `[VERIFIED-RUNTIME: BlueprintTools.read_graph_dsl on
   BP_ThirdPersonCharacter.CastFirebolt, 2026-07-30]`.
4. RE's actual spell contract is separate. `FREAbilityDef` owns
   `ImpactDamage`, `ImpactStatus`, `StatusDuration`, `AoeRadius`, projectile
   physics, and VFX soft paths `[VERIFIED:
   $UEREMCP_LEGACY_PROJECT/Source/RE/Public/REAbilityTypes.h:143-215]`.
   `CastAbility` routes non-authority calls through
   `Server_RequestCastAbility`, and `ResolveAbilityImpact` rejects non-authority
   execution `[VERIFIED:
   $UEREMCP_LEGACY_PROJECT/Source/RE/Private/REPlayerVisualCombatComponent.cpp:3963-3984,5172-5185]`.
   The canonical `fire_s` seed has damage `16`, burn duration `3`, and references
   `/Game/RE/VFX/Magecraft/Spells/NS_Spell_Firebolt`, not the POC B scratch
   system `[VERIFIED:
   $UEREMCP_LEGACY_PROJECT/Saved/magecraft_abilities.json:143-153]`.

Therefore the POC C operation does not start from one composite source whose
networking and damage behaviour can be preserved. The networked marketplace
actor, the RE authority/damage row, and the POC B Niagara source are three
different objects with no variation contract connecting them.

## Required contract before C5 can pass

The owning workstreams must first define a composite source identity rather than
adding more assertions to the current Niagara-only test:

1. **RE source must expose the owner of gameplay behaviour.** A variation request
   needs an explicit source gameplay binding, for example:
   `ability_table + source_row + vfx_phase`, and, only where an actor Blueprint
   actually owns behaviour, `source_actor_blueprint`. Asset-name matching is not
   sufficient.
2. **The requested target relationship must be explicit.** Either:
   - create a target ability row whose protected gameplay fields are copied from
     the source while only presentation fields point at the new ice assets; or
   - bind the new presentation asset to the same existing gameplay row without
     changing that row, if product semantics allow one row to select the
     variation.
3. **UEREMCP must preserve and re-read protected fields.** At minimum for RE:
   `CastType`, projectile physics, `ImpactDamage`, `ImpactStatus`,
   `StatusDuration`, `AoeRadius`, `SpawnEntity`, and the Pattern B cast path.
   For an actor-owned source, also compare CDO replication flags, replicated
   property descriptors, RPC descriptors, and authority/damage graph semantics.
4. **Automated C5 verification must compare before and after snapshots.** It must
   fail on any protected-field drift, missing source binding, unresolved soft
   path, or unverifiable graph area. A modifier string or unchanged source asset
   alone is not proof that the target variation has preserved behaviour.
5. **Run the proof live.** Create the ice variation, save, re-read the target
   gameplay binding and assets, and assert equality of the protected snapshot.
   Multi-client runtime proof is stronger but is not a prerequisite unless
   WS-01 changes C5 to require observed replication rather than verified
   preservation.

Until those contracts exist, `preserve_networking` should be treated as requested
intent only. It must not produce a C5 success claim.

## D5 note

The current D5 wording is already satisfied by the static RE Pattern B checklist:
“static checklist + optional `pie_cast_and_capture`; multi-client net proof is
RB-14 / WS-11” `[VERIFIED: docs/POC_ACCEPTANCE.md:123-130]`. On that wording,
D5 is **MET static** and a multi-client run is not required to claim POC D if
D1-D4 and D6-D8 are met.

If WS-01 separately requires end-to-end observed replication for the overall D
claim, WS-11 must run a listen-server plus remote-client cast that records:
client request reaching server authority, cosmetics observed remotely, and
damage/status changing only on authority. The existing D18 script is only a
partially automated scaffold and explicitly leaves gameplay cases manual
`[VERIFIED:
$UEREMCP_LEGACY_PROJECT/Content/Python/_pie_smoke_listen_d18.py:1-10,269-277]`.

## Acceptance impact

C1-C4 and C6-C7 remain supported by the existing live evidence. C5 remains
**FAIL**, so overall POC C remains **NOT MET**. This proposal does not redefine
C5 and does not upgrade the current `preserve_networking` template modifier into
verified behaviour.
