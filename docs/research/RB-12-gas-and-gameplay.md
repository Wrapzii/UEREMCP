# RB-12: Gameplay Ability System and player-system authoring

- **Owner:** WS-09
- **Status:** complete (research); Phase 4 implementation **not authorized**
- **Blocks:** POC D (batched gameplay ability) — design gate only until Wave 3
- **Priority:** medium-high
- **Last updated:** 2026-07-29
- **Worktree:** `$UEREMCP_ROOT-ws09` @ `ws-09-gameplay`

## Framing

POC D exists to prove the architecture is not Niagara-only. Its value is in the
**multi-asset batch**: one request producing an ability, an effect, tags, a projectile
actor, VFX cues, and replication config, all validated together.

**Critical project fact (answered first):** RE does **not** use Epic GAS
(`UGameplayAbility` / `UGameplayEffect` / `UAbilitySystemComponent`). It uses a
**data-driven magecraft layer**: `FREAbilityDef` rows in `/Game/RE/Data/DT_Abilities`,
executed by `UREPlayerVisualCombatComponent::CastAbility` /
`AuthorityCastAbility`, with cosmetics via multicast and impact authority-only
(D18 Pattern B). Textbook GAS POC shapes in `schemas/examples/batch-fireball-ability.json`
do **not** fit RE as shipped.

Epic `GASToolsets` + `GameplayTagsToolset` **are loaded** in the live RE editor via
`AllToolsets` (aggregator), even though they are not individually listed in
`RE.uproject`. They do **not** author abilities or effects.

## Questions

1. Is GAS actually in use in RE, and how?
2. Can `UGameplayAbility` Blueprint subclasses be created/configured programmatically?
3. `UGameplayEffect`: creatable with modifiers, executions, duration, stacking, granted tags?
4. Attribute sets — inspectable; attributes referenceable in generated effects?
5. Gameplay tags: programmatic add; concurrency under ADR-0006?
6. Ability tasks / targeting / montage — Blueprint graph vs class defaults?
7. Gameplay cues: Niagara/audio binding; seam to WS-07?
8. Replication validation — static vs PIE?
9. Networked ability smoke test — cost / automation (RB-14)?
10. Projectile actors from a spec?
11. Minimum viable `create_spell` + batch plan?
12. *(added)* How do elemental abilities share one parameterized construction pattern?

## Findings

### Q1 — Is GAS in use in RE? (determines everything)

**No. RE does not use Epic's Gameplay Ability System.**

| Evidence | Tag |
|---|---|
| `RE.Build.cs` PublicDependencyModuleNames has `GameplayTasks` but **not** `GameplayAbilities` | `[VERIFIED: $PROJ/Source/RE/RE.Build.cs]` |
| Zero matches for `GameplayAbilities`, `AbilitySystemComponent`, `UGameplayAbility`, `UGameplayEffect`, `GameplayCue` under `$PROJ/Source/RE` and `$PROJ/RE.uproject` | `[VERIFIED: ripgrep 2026-07-29]` |
| Ability model is `FREAbilityDef : FTableRowBase` documented as living in `/Game/RE/Data/DT_Abilities` | `[VERIFIED: $PROJ/Source/RE/Public/REAbilityTypes.h:13-21,85-247]` |
| Runtime loads `AbilityTable` from `/Game/RE/Data/DT_Abilities.DT_Abilities` and resolves casts by row name | `[VERIFIED: $PROJ/Source/RE/Private/REPlayerVisualCombatComponent.cpp:1191,3912-3924]` |
| Cast path: `CastAbility` → `Server_RequestCastAbility` (non-authority) → `AuthorityCastAbility` | `[VERIFIED: REPlayerVisualCombatComponent.cpp:3963-3984]`, `[VERIFIED: RECharacter.h:175-193]` |
| Seed pipeline: `Saved/magecraft_abilities.json` → `Content/Python/build_ability_datatable.py` → DT asset | `[VERIFIED: build_ability_datatable.py:1-41]` |
| REAgentTools CAPABILITY_MATRIX: "No full GAS graph authoring — but `DT_Abilities` + `CastAbility` exist; `pie_cast_and_capture` drives that" | `[VERIFIED: $RAT/Docs/CAPABILITY_MATRIX.md:82]` |
| `REProjectWorkflowTools.get_plugin_project_notes` claims "GAS … not installed; no ability/combat workflow tools" — **half-true**: Epic GAS absent; combat tools exist (`pie_cast_and_capture`) | `[VERIFIED: $RAT/.../project_workflow_tools.py:75-76]` vs `[VERIFIED: capture_workflow_tools.py:835-904]` |

**Architecture summary (RE-native):**

```
magecraft_abilities.json  ──seed──►  DT_Abilities (FREAbilityDef rows)
                                            │
                                            ▼
                         UREPlayerVisualCombatComponent
                         CastAbility / AuthorityCastAbility
                               │
               ┌───────────────┼────────────────┐
               ▼               ▼                ▼
     Multicast cosmetics   Projectile/field   Status / damage
     (unreliable FX)       /wall entities     (authority)
               │               │
               ▼               ▼
     Niagara soft paths /  ARESpellWallActor /
     URESpellVFXDefinition ARESpellFieldActor (replicated specs)
```

`DT_Abilities` is **not** a thin bypass over GAS — it **is** the ability system.
Wheels reference Technique ids (`FName`) only `[VERIFIED: REAbilityTypes.h:15-16]`.

**Element catalog in seed (31 abilities):** Frost 7, Arcane 7, Fire 6, Storm 5,
Nature 4, Holy 2 `[VERIFIED: Saved/magecraft_abilities.json via Python Counter]`.
There is no literal Water/Wind/Earth label; Frost≈water, Storm≈wind, Nature≈earth
for extension mapping.

---

### Q2 — Programmatic `UGameplayAbility` Blueprint create/configure?

**Epic tool ceiling: no ability authoring tools exist.**

`GASToolsets` registers exactly three toolsets `[VERIFIED: GASToolsets.cpp:13-17]`:

| Toolset | Capability | Authoring? |
|---|---|---|
| `UGameplayCueToolset` | list/get/execute cues; create empty cue notify BP; add/remove cue tags | cue notify scaffolding only |
| `UAttributeSetToolset` | `FindAttributeSetClasses`, `ListAttributes` | discovery only |
| `UAbilitySystemInspectorToolset` | runtime ASC attributes/effects/abilities/tags | inspect only; requires ASC |

No `CreateAbility`, no CDO setters for costs/cooldowns/instancing/net execution policy
in `GASToolsets` headers `[VERIFIED: AbilitySystemInspectorToolset.h, AttributeSetToolset.h, GameplayCueToolset.h]`.

**Loaded at runtime:** `list_toolsets` returned
`GASToolsets.GameplayCueToolset`, `AttributeSetToolset`, `AbilitySystemInspectorToolset`
`[VERIFIED-RUNTIME: unreal-mcp list_toolsets 2026-07-29]`. Enabled because
`AllToolsets.uplugin` depends on `GASToolsets` `[VERIFIED: AllToolsets.uplugin:54-56]`
and `RE.uproject` enables `AllToolsets` `[VERIFIED: RE.uproject Plugins]`.

**For RE POC D:** do **not** create `UGameplayAbility` assets. Upsert `FREAbilityDef`
rows. Class-default GAS configuration questions are **out of scope for RE-native
authoring** unless a future ADR explicitly adopts Epic GAS (not proposed here).

Generic Blueprint subclass creation remains possible via Epic `BlueprintTools` /
WS-06 machinery, but that would produce assets **disconnected from RE cast runtime**.

---

### Q3 — `UGameplayEffect` creatable?

**No Epic GASToolsets API creates GameplayEffects.** Inspector can list *active*
effects on an ASC at runtime `[VERIFIED: AbilitySystemInspectorToolset.h:104-111]`.

RE "effects" are:

- Instant/impact fields on `FREAbilityDef` (`ImpactDamage`, `ImpactStatus`,
  `StatusDuration`, `AoeRadius`) `[VERIFIED: REAbilityTypes.h:159-171]`
- Enchant duration + `EffectTag` (`FName`, not `FGameplayTag`)
  `[VERIFIED: REAbilityTypes.h:136-141]`
- `UREStatusEffectComponent` for status application (project C++, not GE)

**Negative:** there is no RE `UGameplayEffect` asset pipeline to author.

---

### Q4 — Attribute sets

Epic `AttributeSetToolset` can discover AttributeSet subclasses if any exist
`[VERIFIED: AttributeSetToolset.h:54-70]`. RE module does not depend on
`GameplayAbilities`, so native RE AttributeSets are not expected.

RE vitals/stamina live in project components (`RECharacterStats`, stamina spend in
`AuthorityCastAbility`) `[VERIFIED: REPlayerVisualCombatComponent.cpp:4047-2065]`.

**Generated GE attribute references:** N/A for RE-native path. If a future GAS path
is adopted, AttributeSet discovery tools are preserve-as-is; effect authoring still
missing.

**Runtime follow-up blocked:** `FindAttributeSetClasses` call failed after editor MCP
transport died (`WinError 10061`) `[VERIFIED-RUNTIME: describe/call refused after
list_toolsets]`. Recorded as negative; header-level conclusion stands.

---

### Q5 — Gameplay tags + ADR-0006 concurrency

**Epic mutation surface exists:**

- `GameplayTagsToolset.AddTag` / `RemoveTag` / `RenameTag` →
  `IGameplayTagsEditorModule::AddNewGameplayTagToINI` / `DeleteTagFromINI` /
  `RenameTagInINI` `[VERIFIED: GameplayTagsToolset.cpp:93-147]`
- `GameplayCueToolset.AddCueTag` / `RemoveCueTag` same INI path
  `[VERIFIED: GameplayCueToolset.cpp:289-338]`

**Concurrency hazard (concrete ADR-0006 case):** tags are written to shared INI
sources (`DefaultGameplayTags.ini` or equivalent). Concurrent agents adding tags
race on the same file; there is no `expected_revision` on the tag table; rename
rewrites referencers project-wide. Stable *asset* paths do not protect INI tag
tables.

**RE mitigation (important):** core ability identity uses **DataTable row names** and
`FName` fields (`AbilityId`, `EffectTag`, `Element` string), **not** `FGameplayTag`
`[VERIFIED: REAbilityTypes.h:91-102,141]`. No `*GameplayTag*` files under
`$PROJ/Config` were found `[VERIFIED: directory listing]`.

**Recommendation for POC D (RE-native):** prefer **not** mutating gameplay tag INIs.
Encode status/element as existing `EREAbilityStatus` / `Element` / `EffectTag` fields.
If cue tags are needed later, namespace under `GameplayCue.UeremcpTests.*` only, with
single-writer policy — raised to WS-01 in
`docs/proposals/ws-09-tag-concurrency-adr0006.md`.

Loaded: `GameplayTagsToolset.GameplayTagsToolset` present in `list_toolsets`
`[VERIFIED-RUNTIME]`.

---

### Q6 — Ability tasks / targeting / montage vs Blueprint graph

**RE cast logic is C++ in `UREPlayerVisualCombatComponent`, not ability Blueprint graphs.**
Cast types are enum-driven (`Projectile`, `GroundTarget`, `SelfCast`, `ChannelBeam`)
`[VERIFIED: REAbilityTypes.h:40-47]`. Targeting/aim: server receives `AimPoint` hint
and re-aims `[VERIFIED: RECharacter.h:177-181]`.

**Blueprint-graph dependency for POC D:**

| Concern | Owner | Needed for RE create_spell? |
|---|---|---|
| Ability event graph (ActivateAbility, tasks) | WS-06 / Epic GAS | **No** — unused |
| Niagara systems for cast/travel/impact | WS-07 | **Yes** — soft paths on row / VFXDefinition |
| Materials / circle MIs | WS-08 | **Yes** — `CircleMaterial`, runtime spell mats |
| Cast montage | WS-10 | **Optional** — not a required `FREAbilityDef` field today; cosmetics are circle/Niagara-driven |

Montage contract with WS-10: optional later; do not block POC D on AnimBP authoring.

---

### Q7 — Gameplay cues ↔ Niagara / audio (WS-07 seam)

**Epic GameplayCue path (available, poorly matched to RE):**

- `CreateCueNotifyAsset` creates a Blueprint subclass of
  `UGameplayCueNotify_Static` or `AGameplayCueNotify_Actor`, sets
  `GameplayCueTag` / `GameplayCueName` on CDO, **does not** bind Niagara/audio
  `[VERIFIED: GameplayCueToolset.cpp:218-286]`
- `ExecuteCueOnSelectedActor` is non-replicated preview
  `[VERIFIED: GameplayCueToolset.cpp:141-182]`

**RE presentation path (actual):**

- Per-row soft paths: `CastNS`, `ProjectileNS`, `ImpactNS`, audio soft paths
  `[VERIFIED: REAbilityTypes.h:199-229]`
- Optional `TSoftObjectPtr<URESpellVFXDefinition>` supersedes legacy soft paths for
  presentation; gameplay retains cast authority
  `[VERIFIED: REAbilityTypes.h:212-215]`, `[VERIFIED: RESpellVFXDefinition.h:11-76]`
- `URESpellVFXDefinition` holds phase Niagara soft refs + color/scalar maps —
  **parameterized VFX contract**, not GameplayCue tags

**Contract proposal for WS-07:** see
`docs/proposals/ws-09-cue-vfx-contract.md`. Primary seam is
`URESpellVFXDefinition` + Niagara soft paths under `/Game/__UeremcpTests/` for POC;
GameplayCueNotify is secondary/out-of-band unless RE adopts cues later.

---

### Q8 — Replication validation (static vs PIE)

RE magecraft networking (D18 Pattern B):

| Mechanism | Role | Tag |
|---|---|---|
| `Server_RequestCastAbility` Reliable + `WithValidation` | client intent; reject None / non-finite aim | `[VERIFIED: RECharacter.h:180-181]`, `[VERIFIED: RECharacter.cpp:352-388]` |
| Rate limit bucket `CastAbility` | anti-spam | `[VERIFIED: RECharacter.cpp:378-382]`, `[VERIFIED: RENetTuningTypes.h:57]` |
| `AuthorityCastAbility` | stamina/cooldown/progression; spawn logic | `[VERIFIED: REPlayerVisualCombatComponent.cpp:3986+]` |
| `Multicast_AbilityCosmetics` **Unreliable** | cast flourish FX only | `[VERIFIED: RECharacter.h:183-185]` |
| Impact damage/status | authority-only (comment in source) | `[VERIFIED: REPlayerVisualCombatComponent.cpp:5181]` |
| `ARESpellWallActor` / `ARESpellFieldActor` | replicated visual/gameplay specs via `OnRep_*` | `[VERIFIED: RESpellWallActor.h:57-77]`, `[VERIFIED: RESpellFieldActor.h:59-75]` |

**Statically checkable (no PIE):**

1. Row exists in `DT_Abilities` after upsert; required fields populated.
2. Soft object paths resolve (`CastNS` / `ProjectileNS` / `ImpactNS` / `VFXDefinition`).
3. For networked entities: `SpawnEntity` ∈ {`spell_wall`,`spell_field`,``}; wall/field
   actor classes exist and have replicated properties (header-level).
4. Unsafe patterns to **flag** (not auto-fix without policy):
   - proposing client-authority damage in a generated Blueprint
   - reliable multicast for high-frequency FX
   - missing validation on proposed custom RPCs
5. Example batch schema's `networking.authority: server` is aspirational for GAS;
   for RE, authority is **already** enforced in C++ cast path — validation is
   "row is consumable by AuthorityCastAbility", not "set NetExecutionPolicy on GA".

**Requires PIE / listen-server:**

1. Client cast → server accept → multicast cosmetics observed on second client.
2. Impact damage applied only on authority.
3. Wall/field actors replicate `OnRep_*` visuals to remote clients.
4. Rate-limit / dead / silenced reject paths.

Master-prompt "unsafe-replication detection" for RE-native spells = static audit of
**proposed** Blueprint/RPC additions + confirmation that damage stays on the existing
authority path. Generating a new GA with `LocalPredicted` would be a regression
relative to RE — and is out of scope.

---

### Q9 — Networked smoke test cost / automation

**Existing prior art:** `RECaptureWorkflowTools.pie_cast_and_capture(ability_id)` —
starts/uses PIE, calls `CastAbility` on player visual combat, waits, captures disk
shot `[VERIFIED: capture_workflow_tools.py:835-928]`, `[VERIFIED: TOOL_CATALOG.md:152]`.

**Cost / risks:**

- AGENT_SYNC notes PIE historically locks the editor; "PIE parked" in places
  `[VERIFIED: $PROJ/AGENT_SYNC.md:39,101]`
- Listen-server multi-client automation is **not** in REAgentTools capture tools
  (single local cast + screenshot)
- Full replication proof needs RB-14 harness (WS-11): listen server + 2 clients, or
  automation test under `UeremcpValidation`

**POC D bar:** static validation + optional single-process `pie_cast_and_capture`
under `/Game/__UeremcpTests/` row id. Multi-client net proof = Phase 4+/RB-14,
not a silent claim of `*_validated` for replication.

---

### Q10 — Projectile / entity actors from a spec?

**Yes, via parameters on `FREAbilityDef`, not per-spell actor subclasses.**

- Projectile physics: `Speed`, `Range`, `ProjRadius`, `GravityScale`, `Homing`
  `[VERIFIED: REAbilityTypes.h:143-157]`
- Optional entities: `SpawnEntity` = `spell_wall` | `spell_field` with dimensions
  `[VERIFIED: REAbilityTypes.h:180-197]`
- Wall/field actors are shared C++ classes parameterized at `Initialize*`
  `[VERIFIED: RESpellWallActor.h:26-36]`, `[VERIFIED: RESpellFieldActor.h:27-36]`

**Do not** generate a new projectile Blueprint per elemental spell for POC D.
Parameterize the shared runtime. New actor classes only if delivery mode is absent
from the enum (escalate via proposal).

Epic `DataTableTools` can create tables and upsert rows
`[VERIFIED: EditorToolset/.../data_table.py:59-241]` — primary mutation substrate for
ability rows.

---

### Q11 — Minimum viable `create_spell` + batch plan

**RE-native `create_spell` specification (minimum):**

| Field | Maps to |
|---|---|
| `name` / `ability_id` | DT row name + `AbilityId` (stable id) |
| `element` | `Element` string + `ElementColor` + `LineId` |
| `delivery` / `cast_type` | `CastType` enum |
| `speed`, `range`, `proj_radius` | physics block |
| `damage`, `aoe_radius`, `status`, `status_duration` | impact block |
| `visual_style` | Niagara soft paths and/or `VFXDefinition` |
| `cast_animation` | optional; defer to WS-10 |
| `networking` | declare `authority: server` + validate against Pattern B (no new RPCs) |
| `tier`, `wheel`, `stamina_cost`, `cooldown_sec` | timing/economy |

**Batch plan (POC D reinterpreted):**

1. `create_vfx_material` / `create_niagara_effect` (WS-08/WS-07) → test paths under
   `/Game/__UeremcpTests/...`
2. Optional `create_spell_vfx_definition` data asset (WS-09) referencing those FX
3. `upsert_ability_row` into a **test** DataTable clone **or** a namespaced row in a
   test-only table — **never** delete/recreate production `DT_Abilities` (see negative
   finding on `build_ability_datatable.py`)
4. `validate_ability` — re-read row; resolve soft refs; static replication checklist
5. Optional `pie_cast_and_capture` if editor healthy

Stable path derivation (ADR-0006):
`/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities` + row `{ability_id}`;
VFX `/Game/__UeremcpTests/VFX/NS_{ability_id}_{phase}`.
`expected_revision` on the DataTable content hash for concurrent row upserts.

Example `schemas/examples/batch-fireball-ability.json` uses `create_gameplay_ability` /
`create_gameplay_effect` — **must be revised by WS-01/WS-05** before POC D coding;
proposal: `docs/proposals/ws-09-poc-d-re-native.md`.

---

### Q12 — Elemental parameterization (fire/water/wind/earth + extensions)

**One semantic operation, many parameter bindings — not per-element primitives.**

Shared construction pattern:

```
create_spell(spec) →
  FREAbilityDef {
    Element, ElementColor, LineId, Tier, CastType,
    physics*, impact*, SpawnEntity?,
    CastNS/ProjectileNS/ImpactNS | VFXDefinition,
    AudioCue*
  }
```

Elemental families in RE today (Fire, Frost, Storm, Nature, Arcane, Holy) differ by
**data**, not by tool. Extensions (Water/Wind/Earth aliases or new schools) = new
rows + VFX assets with the same schema. Wall/field variants flip `SpawnEntity` and
shared actor init flags (`bFire` on wall) `[VERIFIED: RESpellWallActor.h:33-34]`.

**Forbidden design:** `create_fire_spell`, `create_water_spell`, … as separate
agent-facing actions. Internal templates (WS-15) may specialize defaults by element.

---

## Negative findings

1. **Epic GASToolsets cannot author abilities or effects** — inspect + cue notify
   scaffolding only `[VERIFIED: GASToolsets sources]`.
2. **RE does not implement Epic GAS at all** — no module dependency, no ASC usage
   `[VERIFIED: RE.Build.cs + ripgrep]`.
3. **POC example `batch-fireball-ability.json` assumes textbook GAS** — mismatch with
   RE reality `[VERIFIED: schemas/examples/batch-fireball-ability.json]`.
4. **`build_ability_datatable.py` deletes and recreates `DT_Abilities` wholesale**
   `[VERIFIED: build_ability_datatable.py:412-418]` — incompatible with ADR-0006
   idempotent row upsert and "never destroy user content" for production tools.
5. **GameplayCueNotify creation does not wire Niagara/audio** — empty BP + tag only
   `[VERIFIED: GameplayCueToolset.cpp:260-286]`.
6. **Runtime mutation experiments under `/Game/__UeremcpTests/` not completed** —
   after successful `list_toolsets`, subsequent MCP calls got `WinError 10061`
   connection refused; `user-unreal-watch` also unreachable. No assets created.
7. **Multi-client replication automation absent** in REAgentTools; only
   `pie_cast_and_capture` single-cast visual loop.
8. **WS-02 audit matrix for GAS tools still empty** (`docs/audit/epic-toolsets.md`
   seed only) — rows supplied via proposal, not edited into WS-02 paths.
9. **REAgentTools `get_plugin_project_notes` understates combat tooling** (claims no
   ability tools while `pie_cast_and_capture` exists).

## Conditional implementation plan (Phase 4 — not this run)

**Gate:** Wave 3 + WS-01 acceptance of RE-native POC D reinterpretation.

| Step | Work | Depends |
|---|---|---|
| P0 | Land proposals: POC D reshape, tag concurrency, cue↔VFX contract, audit rows | WS-01/02/05/07 |
| P1 | `schemas/domains/gameplay/` — `create_spell`, `upsert_ability_row`, `validate_ability` only; **no** `create_gameplay_ability` unless dual-mode ADR | WS-09 + WS-05 review |
| P2 | `UeremcpGameplay` toolset: deterministic path derivation; DataTable row upsert via Epic `DataTableTools` or C++ equivalent; dry_run default for deletes | WS-03 plugin base |
| P3 | Batch with WS-07/WS-08 artifacts under `/Game/__UeremcpTests/`; atomic transaction | WS-05/11 |
| P4 | Static replication checklist in `validate_ability`; honest statuses | WS-09 |
| P5 | Optional PIE smoke via existing capture pattern; multi-client deferred to RB-14 | WS-11 |
| P6 | Promote elemental templates to WS-15 only after POC D green | WS-15 |

**Do not implement** Epic-GAS-first tools that ignore `CastAbility` / `DT_Abilities`.

## Deliverables checklist

- [x] Written description of RE's actual ability architecture (this brief §Q1)
- [ ] `schemas/domains/gameplay/` — **deferred** (Phase 4; not authorized this run)
- [ ] POC D batch — **deferred** (blocked on Wave 3 + proposal acceptance)
- [x] Replication validation checklist — §Q8
- [x] Cue↔VFX contract drafted for WS-07 — `docs/proposals/ws-09-cue-vfx-contract.md`
- [x] Montage note for WS-10 — optional; not blocking (proposal cross-link)
- [x] Gameplay-tag concurrency hazard raised — `docs/proposals/ws-09-tag-concurrency-adr0006.md`
- [x] Audit row proposal for WS-02 — `docs/proposals/ws-09-audit-gas-toolsets.md`
- [x] POC D RE-native reshape — `docs/proposals/ws-09-poc-d-re-native.md`

## Open questions / blockers

1. **WS-01:** Accept RE-native POC D (DT row + VFX + validate) vs require Epic GAS
   assets that RE cannot execute?
2. **WS-05:** Revise example batch schema / action names for gameplay domain.
3. **WS-02:** Merge GAS/GameplayTags audit rows from proposal.
4. **Editor health:** MCP transport flaked; need stable editor for
   `/Game/__UeremcpTests/` runtime proofs.
5. **WS-11 / RB-14:** Listen-server automation for D5 "replication validated".
6. **Production DT policy:** never use delete+recreate seeder from agent tools;
   confirm test-table vs namespaced row strategy with project owners.
