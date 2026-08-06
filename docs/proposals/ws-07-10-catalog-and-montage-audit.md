# WS-07 / WS-10 — capability audits for `describe_niagara_catalog` and `submit_montage_sections`

**Status:** implemented on `ws-06-07-10-tool-expansion`.
**Owner of the destination file:** WS-02 (`docs/audit/epic-toolsets.md`). Rule 3 proposal route.

Companion to [`ws-06-node-catalog-audit.md`](ws-06-node-catalog-audit.md).

---

## 1. `niagara.describe_niagara_catalog` — scope corrected by the audit

### What the audit found

The first draft of this tool was written as a *module discovery* tool. The rule 2 audit
killed that framing, and the tool's documentation was rewritten before it shipped. Epic's
Niagara toolsets already do module discovery, and do it better
`[VERIFIED: docs/audit/raw/schemas/NiagaraToolsets.NiagaraToolset_Assets.json]`
`[VERIFIED: docs/audit/raw/schemas/NiagaraToolsets.NiagaraToolset_System.json]`:

| Epic tool | What it does | Why it beats a hand-rolled catalog |
|---|---|---|
| `NiagaraToolset_Assets.FindNiagaraScripts` | Searches **all** `UNiagaraScript` assets by folder, name, usage, visibility, module-usage bitmask | Reads asset-registry tags with **no `LoadObject`**; covers the whole library, not a shortlist |
| `NiagaraToolset_System.GetModuleSchemaFromAsset` | Returns a module asset's **input schema**, standalone, no system context needed | Exactly the per-module input names an authoring agent needs |
| `NiagaraToolset_System.GetModuleTopology` | Module metadata + all inputs (name/type/visibility) for a module in a stack | — |
| `NiagaraToolset_System.GetAvailableDynamicInputs` | Dynamic-input modules compatible with a given type | — |

**Correction to an earlier claim.** An in-progress version of the tool's header stated
that per-module input names could not be returned because
`UNiagaraExternalEditUtilities::GetModuleInputValues` requires a
`FNiagaraExt_StackItemReference` and the `UNiagaraScript` overload is commented out
`[VERIFIED: NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:1247,1287]`.
That is true of *that C++ entry point* but false as a capability statement — Epic exposes
the capability through `GetModuleSchemaFromAsset`. The shipped tool no longer makes the
stronger claim and points at Epic's tool instead.

### What is genuinely not covered by Epic

The `primitive_id` vocabulary is **UEREMCP's own contract surface**. When an agent calls
`CreateNiagaraEffect` with `modules[{primitive_id: "spawn_rate"}]`, that string is
resolved by a private alias table in
`UeremcpNiagaraModuleResolve.cpp` — invisible to every Epic tool. The same holds for:

- the renderer hints `ResolveRendererClass` accepts (`sprite`/`mesh`/`ribbon`/`light`)
- the script usages `NormalizeScriptUsage` resolves
- the stack-input value modes `FUeremcpNiagaraStackInputs::TryBuildStackInputValue` accepts
  (`local`/`linked`/`hlsl_expression`/`data_interface`/`dynamic_input`/`enum`)

Before this tool, an agent could only learn these by triggering the resolver's error
string. That is the gap, and it is the whole justification.

To stop the advertised vocabulary drifting from the accepted vocabulary, the alias table
was lifted into `UeremcpNiagaraModuleResolve::ModuleAliasTable()` and is now the single
source for both resolution and the catalog. `UEREMCP.Niagara.Catalog.ResolverAgreement`
asserts every advertised `primitive_id` round-trips through the real resolver to the
advertised asset.

### Proposed matrix row

| Capability | Epic tool(s) | Disposition | Superseded by | Limitation |
|---|---|---|---|---|
| Niagara module discovery + module input schema | `FindNiagaraScripts`, `GetModuleSchemaFromAsset`, `GetModuleTopology` | **preserve — do not supersede** | — | none relevant; these are better than anything UEREMCP should build |
| UEREMCP `primitive_id` / renderer-hint / script-usage / input-mode vocabulary | **none — not Epic surface** | new | `niagara.describe_niagara_catalog` | curated shortlist, not the full library; points at Epic for both |

### Limitations (stated)

- `modules[]` is a 16-row curated shortlist, not a library browse. Said plainly in the
  tool description and capability notes.
- `verify_assets: true` costs one `LoadObject` per row. Default is true because a stale
  row is worse than the load.
- Does not return module inputs, by design (see above).

---

## 2. `animation.submit_montage_sections` — fills a gap the audit already recorded

### What the audit found

There is **no Epic equivalent**, and `docs/audit/epic-toolsets.md` already says so:

- Line 287: AnimationAssistant `Sequencer*` (276 tools) — *"No AnimBP/montage/notify"*
- Line 296, gap table: *"Goal-level montage + **real** AnimNotify authoring |
  `UAnimationBlueprintLibrary` exists; no Epic toolset exposes notify authoring | **WS-10**"*
- Line 289: `UAnimationBlueprintLibrary` — *"not a toolset"*, disposition **internalise**

I checked the library's montage surface directly rather than trusting the row.
`UAnimationBlueprintLibrary` exposes exactly one montage function, and it is read-only:

```
/** Retrieves the Names of the Animation Slots used in the given Montage */
static UE_API void GetMontageSlotNames(const UAnimMontage*, TArray<FName>& SlotNames);
```
`[VERIFIED: Editor/AnimationBlueprintLibrary/Public/AnimationBlueprintLibrary.h:83-85]`

No section authoring anywhere. Before this change the UEREMCP Animation domain was also
entirely read-only — `InspectMontage` and `ReadAnimBp`, nothing else. This is the domain's
first write path.

### Verified API surface

| Claim | Tag |
|---|---|
| `UAnimMontage::CompositeSections` | `[VERIFIED: Engine/Classes/Animation/AnimMontage.h:697]` |
| `UAnimMontage::GetSectionIndex(FName)` | `[VERIFIED: Engine/Classes/Animation/AnimMontage.h:819]` |
| `UAnimMontage::AddAnimCompositeSection(FName, float)` → index or `INDEX_NONE`, `WITH_EDITOR` | `[VERIFIED: Engine/Classes/Animation/AnimMontage.h:906]` |
| `UAnimMontage::DeleteAnimCompositeSection(int32)` → bool, `WITH_EDITOR` | `[VERIFIED: Engine/Classes/Animation/AnimMontage.h:912]` |
| `FCompositeSection::SectionName` / `NextSectionName` | `[VERIFIED: Engine/Classes/Animation/AnimMontage.h:42,53]` |
| `FAnimLinkableElement::GetTime` / `SetTime` | `[VERIFIED: Engine/Classes/Animation/AnimLinkableElement.h:77,83]` |
| `EAnimLinkMethod::Absolute` | `[VERIFIED: Engine/Classes/Animation/AnimLinkableElement.h:20]` |
| `UAnimSequenceBase::GetPlayLength()` | `[VERIFIED: Engine/Classes/Animation/AnimSequenceBase.h:86]` |
| `UeremcpAnimation` is an `Editor` module, so `WITH_EDITOR` APIs are available | `[VERIFIED: Plugins/UEREMCP/UEREMCP.uplugin:40-41]` |

### Design notes

- **Complete-state, not per-section pokes** (ADR-0004). `sections[]` is the whole desired
  set; unlisted sections are removed. This is what makes it one call instead of a
  read → add → wire → verify loop.
- **`dry_run` defaults true** because removal is destructive (AGENTS.md rule 8, ADR-0010).
  The caller must pass `dry_run: false`.
- **`modified_and_validated` requires a post-write re-read** (rule 6) confirming every
  section's start time and chaining, plus that removals actually took effect. Any mismatch
  yields `failed_validation` with the specific differences, never silent success.
- **`expected_revision`** is checked against the `InspectMontage` content hash before
  anything is touched (ADR-0006), and the response revision comes from the same service,
  so it is comparable with an `InspectMontage` revision.

### Proposed matrix row

| Capability | Epic tool(s) | Disposition | Superseded by | Limitation |
|---|---|---|---|---|
| Montage section authoring | **none** (`UAnimationBlueprintLibrary::GetMontageSlotNames` is read-only) | new | `animation.submit_montage_sections` | sections only — notifies, slots and segments not covered |

### Limitations (stated)

- **Sections only.** Notifies, slot tracks and segments are untouched. The audit's line-296
  gap ("montage + **real** AnimNotify authoring") is therefore only half closed; notify
  authoring remains open for WS-10.
- **No mutating-gate integration.** `UeremcpAnimation` does not depend on `UeremcpCore` /
  `UeremcpSecurity`, so unlike `submit_graph` this does not pass through
  `FUeremcpMutatingDispatch` (permission tier, mutator queue, audit log). It relies on
  `dry_run` defaulting true. Wiring it to the ADR-0010 gate is the obvious follow-up and
  should happen before this is used outside scratch paths.
- **No path-root restriction.** `submit_graph` confines writes to scratch roots; this does
  not. Combined with the point above, treat it as sandbox-grade until gated.
- **Not yet run in a live editor.** Automation tests are written but unexecuted.
