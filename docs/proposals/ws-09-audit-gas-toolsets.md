# Proposal: Audit rows for GASToolsets + GameplayTagsToolset

- **From:** WS-09
- **To:** WS-02 (owns `docs/audit/**`)
- **Date:** 2026-07-29
- **Status:** proposed
- **Evidence:** local headers + `list_toolsets` runtime; full brief in RB-12

> WS-09 does not edit `docs/audit/**`. Please merge these rows when filling
> `epic-toolsets.md`.

## Load state

| Fact | Tag |
|---|---|
| `GASToolsets` / `GameplayTagsToolset` not individually listed in `RE.uproject` | `[VERIFIED: RE.uproject]` |
| Both enabled as dependencies of `AllToolsets` | `[VERIFIED: AllToolsets.uplugin:50-56]` |
| Both visible in live `list_toolsets` | `[VERIFIED-RUNTIME: unreal-mcp 2026-07-29]` |

## Suggested matrix rows

| Toolset | Tool | Purpose | Limitations | Altitude | Disposition | Superseded by | Tag |
|---|---|---|---|---|---|---|---|
| GASToolsets.GameplayCueToolset | ListCues / GetCueInfo / FindCueNotifyAssets / FindCueTagsWithoutNotifies | Inspect cue tags + notify assets | Read-only discovery | primitive | **preserve** | — | `[VERIFIED: GameplayCueToolset.h:67-145]` |
| GASToolsets.GameplayCueToolset | ExecuteCueOnSelectedActor | Non-replicated cue preview on selection | Needs selection; not net validation | primitive | **preserve** | — | `[VERIFIED: GameplayCueToolset.h:91-92]` |
| GASToolsets.GameplayCueToolset | CreateCueNotifyAsset | Create empty Static/Actor notify BP + set tag on CDO | **No Niagara/audio bind**; tag must pre-exist | primitive | **improve** (compose into goal-level with VFX) | WS-09 `create_spell` presentation + WS-07 | `[VERIFIED: GameplayCueToolset.cpp:218-286]` |
| GASToolsets.GameplayCueToolset | AddCueTag / RemoveCueTag | Mutate GameplayCue.* tags in INI | Concurrent-agent INI race; destructive | primitive | **preserve** with policy | ADR-0006 tag policy | `[VERIFIED: GameplayCueToolset.cpp:289-338]` |
| GASToolsets.AttributeSetToolset | FindAttributeSetClasses / ListAttributes | Discover AttributeSet types | No create/edit; RE has no GAS attrs | primitive | **preserve** | — | `[VERIFIED: AttributeSetToolset.h:54-70]` |
| GASToolsets.AbilitySystemInspectorToolset | GetAttributeValues / GetActiveEffects / GetGrantedAbilities / GetActiveTags | Runtime ASC inspect | Requires ASC; RE characters have none | primitive | **preserve** (useless for RE magecraft) | — | `[VERIFIED: AbilitySystemInspectorToolset.h:101-131]` |
| GameplayTagsToolset | ListTags / GetTagInfo / FindReferencersByTag | Tag discovery | — | primitive | **preserve** | — | `[VERIFIED: GameplayTagsToolset.h:44-89]` |
| GameplayTagsToolset | AddTag / RemoveTag / RenameTag | INI tag mutation | Concurrency hazard; rename rewrites refs | primitive | **preserve** with policy | ADR-0006 | `[VERIFIED: GameplayTagsToolset.cpp:93-147]` |
| editor_toolset.DataTableTools | create / add_rows / set_rows / get_rows / list_rows / … | DataTable CRUD | Whole-table JSON rewrite on set; no revision API | mid | **preserve** — substrate for RE ability rows | WS-09 `upsert_ability_row` wraps with idempotency | `[VERIFIED: data_table.py:59-241]` |

## Real gaps (for audit "Real gaps" section)

- No Epic tool creates/configures `UGameplayAbility` or `UGameplayEffect`.
- No Epic tool understands RE `FREAbilityDef` / `CastAbility`.
- Goal-level `create_spell` batch is therefore **not** a rename of GASToolsets —
  it must wrap DataTable + VFX domains and validate against RE runtime.

## REAgentTools disposition (gameplay)

| Item | Disposition | Note |
|---|---|---|
| `pie_cast_and_capture` | **preserve / reuse** | Validation smoke for CastAbility |
| `get_plugin_project_notes` GAS line | **improve** (docs accuracy) | Says no ability tools; capture tool exists |
| GAS graph authoring | N/A | Correctly out of scope; RE isn't GAS |

## Response (WS-01)

**Routed to WS-02.** Please fold these rows into `docs/audit/epic-toolsets.md`
when convenient. Disposition guidance matches ADR-0010 (tag INI = preserve with
policy) and accepted RE-native POC D (ASC inspector preserve but useless for
magecraft).
