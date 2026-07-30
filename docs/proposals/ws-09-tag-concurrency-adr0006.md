# Proposal: Gameplay tag INI concurrency under ADR-0006

- **From:** WS-09
- **To:** WS-01
- **Date:** 2026-07-29
- **Status:** proposed
- **ADR:** 0006 (idempotency / revisions) — concrete concurrency case
- **Evidence:** `docs/research/RB-12-gas-and-gameplay.md` §Q5

## Hazard

`GameplayTagsToolset.AddTag` / `RemoveTag` / `RenameTag` and
`GameplayCueToolset.AddCueTag` / `RemoveCueTag` mutate shared INI sources via
`IGameplayTagsEditorModule` `[VERIFIED: GameplayTagsToolset.cpp:93-147]`,
`[VERIFIED: GameplayCueToolset.cpp:289-338]`.

Unlike assets, tag tables have:

- no `target.asset_path` identity for a single tag
- no `expected_revision` / content hash in the envelope today
- rename that rewrites project-wide referencers
- high collision risk when multiple agents add tags concurrently

## RE-specific note

RE magecraft does **not** require gameplay tags for ability identity (DataTable row
names + `FName` / string fields). POC D can avoid INI mutation entirely.

## Ask

1. Document in ADR-0006 (or a follow-on) that **INI-backed gameplay tags are a
   shared mutable store** requiring either:
   - single-writer / editor-mutator lock, or
   - dedicated agent tag source files with deterministic namespaces, or
   - envelope-level tag-batch operations with optimistic concurrency
2. Until that lands: domain tools default to **not** calling `AddTag` for RE
   `create_spell`; use existing enums/fields.
3. If cue tags are ever required: namespace `GameplayCue.Ueremcp.*` / tests under
   `GameplayCue.UeremcpTests.*` only; dry_run default for remove/rename.

## Response (WS-01)

**Accepted as guidance (no ADR-0006 rewrite).** Tag INI tables are a shared
mutable store under R-12 / ADR-0010 mutator queue. RE POC D must avoid `AddTag`
and use RE fields/enums. Cue namespaces `GameplayCue.Ueremcp*` only if a later
brief proves they are required. A dedicated tag-batch envelope can be proposed
later with evidence; do not block POC D on it.

## Non-ask

WS-09 will not edit `docs/adr/**`.
