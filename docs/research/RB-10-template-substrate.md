# RB-10: Template and pattern library substrate

- **Owner:** WS-15
- **Status:** **research_complete** (Phase 1 gates still open — no implementation)
- **Blocks:** ADR-0008, POC C (variation from existing effect)
- **Priority:** high
- **Last verified:** 2026-07-29 against UE 5.8 install at
  `$UE_ROOT/Engine`

## Executive summary — ADR-0008 recommendation

**Do not subclass `UAgentSkill` as the template substrate.** Use a **structured adjacent
asset**: JSON documents conforming to the frozen `schemas/template-library/template.schema.json`,
loaded by `UeremcpTemplates` into an in-memory registry, with optional future
`UPrimaryDataAsset` editor wrappers.

**Compose with Epic, do not fork it:**

| Concern | Epic primitive | UEREMCP use |
|---|---|---|
| Prompt-shaped agent guidance | `UAgentSkill` + `GeneratePrompt` | Optional companion skill per high-value template (explains, does not execute) |
| Asset similarity search | `SemanticSearchToolset` | Augment `search_templates` for *produced assets* and promotion discovery; not primary template index |
| Skill CRUD lifecycle | `UAgentSkillToolset` | Do not duplicate; reference in audit as preserve |
| Execution | `execute_plan` batch operations | `construction_plan` steps are batch ops — single interpreter |

Full recommendation for WS-01: [`docs/proposals/ws-15-adr-0008-recommendation.md`](../proposals/ws-15-adr-0008-recommendation.md).

---

## Framing

Master prompt §10 wants a general library storing asset, graph, system, gameplay,
Niagara, material, animation, Blueprint, replication, UI, and AI patterns — searchable,
versioned, inheritable, composable, promotable from successful assets, and
**authorable by the agent**.

Epic ships something adjacent. `UAgentSkillToolset` exposes four `AICallable`
statics — `ListSkills`, `GetSkills`, `CreateSkill`, `UpdateSkill` — over `UAgentSkill`
UObject assets `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h:58-104]`.

The difference: `UAgentSkill` is **prompt-shaped** — it has
`GeneratePrompt(InitialInstructions)` returning a string
`[VERIFIED: AgentSkill.h:42-48]`. Our templates are **execution-shaped** —
`construction_plan` is an array of batch operations, plus `validation_rules`,
`typical_ranges`, and `known_failure_cases`
(`schemas/template-library/template.schema.json`).

---

## Evidence: `UAgentSkill` surface area

### `FAgentSkillDetails`

```cpp
USTRUCT(BlueprintType)
struct FAgentSkillDetails
{
    UPROPERTY(BlueprintReadWrite, Category = "AgentSkill")
    FString Instructions;
};
```

`[VERIFIED: AgentSkill.h:17-26]`

**Finding:** one string field. No structured payload, no version, no domain, no
`construction_plan`, no `inputs` schema. `GetSkills` returns only this struct
`[VERIFIED: AgentSkill.h:77-78, AgentSkill.cpp:60-85]`.

### `UAgentSkill` class

- `Description` — `EditDefaultsOnly`, multiline string `[VERIFIED: AgentSkill.h:34-36]`
- `Instructions` — protected, `EditDefaultsOnly` `[VERIFIED: AgentSkill.h:50-52]`
- `GetDetails()` — calls `GeneratePrompt(Instructions)` and returns `FAgentSkillDetails`
  `[VERIFIED: AgentSkill.cpp:19-24]`
- `GeneratePrompt` — `BlueprintNativeEvent`; default implementation returns
  `InitialInstructions` unchanged `[VERIFIED: AgentSkill.h:43-48]`
- `Blueprintable`, inherits `UObject` (not `UDataAsset`, not `UPrimaryDataAsset`)
  `[VERIFIED: AgentSkill.h:28-29]`
- Subclassing is supported — test fixture `UAgentSkillCustomPrompt` overrides
  `GeneratePrompt_Implementation` `[VERIFIED: AgentSkillsTest.h:11-21]`

**Finding:** subclassing can customise prompt generation, but the toolset API
(`ListSkills`, `GetSkills`) never surfaces subclass-specific properties — only
`Description` (list) and `Instructions` via `GetDetails` (get). A subclass with
extra UPROPERTY fields would be invisible to `GetSkills` unless we bypass Epic and
call `UToolsetLibrary::GetObjectProperties` separately
`[VERIFIED: ToolsetLibrary.h:58-59]`.

### Storage and discovery

**Where skills live:** Blueprint assets whose generated class derives from
`UAgentSkill`. `CreateSkill` uses `UBlueprintFactory` with
`ParentClass = UAgentSkill::StaticClass()` and writes to `FolderPath` (must start
with `/`) `[VERIFIED: AgentSkill.cpp:88-130]`.

**Discovery:** `ListSkills` calls `UToolsetLibrary::GetDerivedClasses(UAgentSkill::StaticClass())`,
loads each derived class, skips empty descriptions, returns
`TMap<FString, FString>` of class path → description
`[VERIFIED: AgentSkill.cpp:26-57]`.

**No search, no similarity, no versioning, no inheritance.** Full enumeration only.
Skills with empty `Description` are omitted from `ListSkills`
`[VERIFIED: AgentSkill.cpp:51-54]`.

### `CreateSkill` / `UpdateSkill` behaviour

| Operation | Accepts | Writes | Restrictions |
|---|---|---|---|
| `CreateSkill` | `FolderPath`, `AssetName`, `Description`, `FAgentSkillDetails` | New Blueprint asset; returns generated class path | Folder must exist and start with `/`; rolls back on `UpdateSkill` failure `[VERIFIED: AgentSkill.cpp:88-130]` |
| `UpdateSkill` | `SkillPath`, `Description`, `FAgentSkillDetails` | Mutates CDO `Description` + `Instructions`; marks package dirty | Refuses native C++ classes (`CLASS_Native`) and transient classes (`RF_Transient`) `[VERIFIED: AgentSkill.cpp:133-168, AgentSkillsTest.cpp:126-162]` |

Doc comment on both mutators: *"This should ONLY be called after getting explicit
direction or permission from the user."*
`[VERIFIED: AgentSkill.h:82-83, 94-95]`

**Finding:** Epic treats agent skill authoring as permission-gated. UEREMCP should
mirror this for `promote_to_template` and agent-authored template writes.

### Security filtering (Epic built-in)

`UToolsetRegistrySettings` exposes:

- `AgentSkillBlockedNames` — regex or substring patterns; block wins
- `AgentSkillAllowedNames` — allowlist when non-empty

Applied in both `ListSkills` and `GetSkills`
`[VERIFIED: ToolsetRegistrySubsystem.h:42-53, AgentSkill.cpp:28-32, AgentSkillsTest.cpp:234-302]`.

**Finding:** path-based visibility control exists for skills. UEREMCP should use
analogous allowed-root patterns for template storage (proposal to WS-12), and hide
internal/shipped templates from agent enumeration via path regex.

---

## Evidence: `SemanticSearchToolset`

Two `AICallable` tools: `Search` (natural-language query, hybrid vector + BM25) and
`FindSimilar` (vector-only, asset-to-asset)
`[VERIFIED: SemanticSearchToolset.h:55-79]`.

**Indexed asset classes** (per header comment): Blueprint, StaticMesh, SkeletalMesh,
Texture, Material, MaterialInstance
`[VERIFIED: SemanticSearchToolset.h:47-48]`.

**Filters:** `ClassFilter`, `PathRegexes` (regex on soft object path), `K` (top-k)
`[VERIFIED: SemanticSearchToolset.h:44-52]`.

**Not indexed:** JSON files on disk, `UAgentSkill` unless stored as Blueprint assets
in indexed Content Browser folders, arbitrary `UDataAsset` subclasses unless the
SemanticSearch plugin registers a processor for them
`[VERIFIED: SemanticSearchToolset.cpp:214-214 — supported classes from FAssetProcessorManager]`.

**Implications for WS-15:**

| Use case | SemanticSearch fit |
|---|---|
| "Find templates like fire projectile" | **Poor alone** — JSON templates are not in the index |
| "Find assets similar to this promoted fireball" | **Good** — `FindSimilar` on `/Game/.../NS_Fireball` |
| "Search templates by natural language" | **Requires UEREMCP `search_templates`** over template metadata (`search_terms`, `description`, `domain`, `category`) |
| Hybrid discovery | Compose: `search_templates` first; if insufficient, `SemanticSearchToolset.Search` with `PathRegexes: ["^/Game/__UeremcpPoc/.*"]` on produced assets |

**Negative finding:** delegating template similarity entirely to `SemanticSearchToolset`
without a UEREMCP metadata index would fail for JSON-stored templates and would
conflate *asset* similarity with *pattern* similarity. Promotion workflow needs both.

---

## Evidence: REAgentTools prior art

REAgentTools has **no template library**. Closest prior art:

| Artifact | Relevance | Tag |
|---|---|---|
| `REBatchWorkflowTools.execute_editor_batch` | `$ref` chaining, `dry_run`, allowlisted actions — structural ancestor of `construction_plan` | `[VERIFIED: REAgentTools/Content/Python/re_agent_tools/toolsets/batch_workflow_tools.py:17-26, 64-70]` |
| `RENiagaraWorkflowTools` | Placement/params only; no graph DSL, no templates | `[VERIFIED: docs/audit/reagenttools.md:92]` |
| Workflow composites generally | Goal-level single-call patterns worth promoting | `[VERIFIED: docs/audit/reagenttools.md:71-83]` |

**Negative finding:** no existing template search, instantiate, promote, or variation
machinery in REAgentTools. WS-15 is net-new capability, not a migration.

---

## Decision analysis: subclass vs adjacent asset

### Option A — Subclass `UAgentSkill`

| Pro | Con |
|---|---|
| Reuses `ListSkills`/`CreateSkill`/`UpdateSkill` | `GetSkills` returns only `Instructions` string — structured template payload invisible |
| Blueprint authoring UX | `construction_plan` (array of batch ops) does not map to UPROPERTY without custom JSON-in-string hack |
| `GeneratePrompt` for agent explanation | Forces prompt-shaped mental model on execution-shaped data |
| | No versioning, `inherits_from`, `supported_modifiers` in Epic type |
| | Subclass properties not surfaced by Epic toolset API |
| | Couples template lifecycle to Experimental `ToolsetRegistry` churn |

**Verdict: rejected as primary substrate.**

### Option B — `UPrimaryDataAsset` subclass

| Pro | Con |
|---|---|
| Native Unreal asset, hot reload, Content Browser | Schema validation harder than JSON-on-disk |
| Could register SemanticSearch processor (future) | WS-01 schema is JSON Schema, not USTRUCT — dual maintenance |
| | Agent authoring requires asset factory, not plain JSON write |

**Verdict: viable Phase 2 editor wrapper; not canonical store.**

### Option C — JSON files + in-memory registry (recommended canonical)

| Pro | Con |
|---|---|
| Matches frozen `template.schema.json` exactly | No hot reload without file watcher |
| `tools/validate_schemas.py` validates at commit time | Not in SemanticSearch index without extra work |
| Git-diffable, reviewable in PRs | Runtime authoring needs write-to-disk + reload |
| `templates/README.md` already defines layout | |
| `construction_plan` items are batch ops — one executor | |

**Verdict: accepted as canonical store for ADR-0008.**

### Option D — Compose with `UAgentSkill` (recommended complement)

For high-traffic templates, optionally generate a companion `UAgentSkill` Blueprint
that:

- `Description` = template one-liner
- `Instructions` / `GeneratePrompt` = human-readable instantiation guide derived from
  `inputs`, `typical_ranges`, `known_failure_cases`
- Does **not** hold `construction_plan` — references `template_id` in Instructions

Cheap, uses Epic discovery for "how do I use this pattern?" without making Epic
own execution. **Not required for POC C.**

---

## Search / index / instantiate / promote semantics

### `search_templates` (planned — WS-15)

**Input:** natural-language query, optional `domain`, `category`, `element` filter,
`limit`.

**Index (in-memory, built at module startup):**

1. Load all `templates/**/*.json`; validate against `template.schema.json`.
2. Index fields: `template_id`, `domain`, `category`, `description`, `purpose`,
   `search_terms[]`, `supported_modifiers[]`, `inputs` property names.
3. Score: token overlap + domain/category boost; Phase 2 optional embedding over
   concatenated metadata (not blocking POC C).

**Does not replace** Epic `list_toolsets` or `SemanticSearchToolset` — different
object type (construction patterns vs assets vs tools).

### `instantiate_template` (planned — WS-15)

**One semantic operation** per ADR-0003/0005:

```
instantiate_template({
  template_id: "niagara.projectile.elemental.v1",
  inputs: { element: "water", scale: 1.2, intensity: 0.8 },
  modifiers: ["crystalline_fragments", "reduce_trail_persistence"],
  preserve: ["networking", "damage"],
  target: { asset_path: "/Game/VFX/Spells/NS_Waterbolt" },
  mode: "create_or_update",
  idempotency_key: "..."
})
```

**Algorithm:**

1. Resolve template chain: `inherits_from` → merge parent fields (child overrides).
2. Apply `inputs` against template `inputs` JSON Schema (subset validation).
3. Resolve `modifiers` against `supported_modifiers` — each modifier is a named
   delta spec (see Elemental variation below).
4. Materialise `construction_plan`: substitute `$inputs`, `$refs`, element presets
   into batch operation `specification` fields.
5. Delegate to `execute_plan` with `validation_rules` appended as post-steps.
6. Return envelope with `template_used`, `inherited_from`, `modifiers_applied`,
   `overrides`, per ADR-0006 `expected_revision` on template store if mutating.

**Negative finding:** `instantiate_template` cannot ship until WS-05 `execute_plan`
and at least one domain constructor (WS-07 POC B) exist. Research-only is correct
for Phase 1.

### `promote_to_template` (planned — WS-15)

**Input:** `source_asset` path, optional `base_template_id`, `template_id` for output.

**Algorithm (feasible, non-trivial):**

1. Retrieve complete structured state of source asset (domain `get_*` actions).
2. If `base_template_id` provided: diff retrieved state against
   `instantiate_template(base, default_inputs)` output — structural equality on graph
   topology, parameter deltas on scalars/colours/curves.
3. Emit new template JSON:
   - `authored_by: "promoted_from_asset"`
   - `promoted_from: <source_asset>`
   - `construction_plan` = minimised plan that reproduces diff from base (or full plan
     if no base)
   - New `supported_modifiers` entries for each named delta discovered
   - `known_failure_cases` empty initially
4. Validate against schema; write to `templates/<domain>/` (or agent quarantine path).
5. Default `dry_run: true` until human or validation gate approves.

**Risk:** diff minimisation quality depends on WS-06/07 read fidelity. Promotion from
assets without a base template produces a **copy-shaped** template — fails POC C7
unless abstracted. Promotion **with** a base template is the intended path.

---

## Third-generation determinism (POC C7)

POC C requires: fireball → ice variation (gen 2) → third variation (gen 3) proving
pattern not copy `[VERIFIED: docs/POC_ACCEPTANCE.md:96-99]`.

**Mechanism:**

| Generation | Operation | What proves "pattern" |
|---|---|---|
| 1 | POC B `create_niagara_effect` or seed template | Baseline |
| 2 | `instantiate_template(fireball, {element: ice}, modifiers: [...])` | Response shows `inherited_from` + `modifiers_applied`, not full graph duplication |
| 3 | `instantiate_template(fireball, {element: earth}, modifiers: [...])` | Different inputs/modifiers, same `template_id`, new asset path |

**Determinism requirements (ADR-0006 alignment):**

- Same `template_id` + same `inputs` + same `modifiers` + same `target.asset_path`
  → `no_change_required` or idempotency replay.
- `template_id` includes `.v<n>` — bump `version` integer on incompatible plan changes.
- Template store updates use `expected_revision` = hash of canonical template JSON.

**Negative finding:** third-generation proof is **blocked** on WS-07 POC B and
`execute_plan`. Template substrate design is unblocked; POC C execution is not.

---

## Validation rules

Three layers:

| Layer | When | What |
|---|---|---|
| **Schema** | Load / commit | `tools/validate_schemas.py` against `template.schema.json` |
| **Plan** | Instantiate | Each `construction_plan` step validates against `batch/plan.schema.json#/$defs/operation` |
| **Instance** | Post-execute | Template `validation_rules[]` → `validation.checks_performed` in response |

Template `validation_rules` items: `{ rule_id, check, severity?, message? }`
`[VERIFIED: template.schema.json:88-101]`.

**`check` grammar (proposed for ADR-0008 implementation):** reference strings
interpreted by `UeremcpValidation` — e.g. `niagara.emitters_non_empty`,
`niagara.renderers_bound`, `blueprint.compiles`, `gameplay.tags_present`. Domain WS
owns check implementations; template library owns orchestration.

**Failure recording loop (question 10):**

| Trigger | Writer | Field |
|---|---|---|
| `failed_validation` on instantiate | Automatic | Append to `known_failure_cases` on **quarantine copy** of template, not shipped core |
| Agent/human diagnosis | `update_template` (future) | Symptom, cause, resolution, `engine_version` |
| Promotion | `promote_to_template` | Empty `known_failure_cases` — failures accumulate in operation |

Automatic writes go to agent-authored or promoted templates only; core shipped
templates in `templates/` repo path require PR (human review).

---

## Versioning and revisions

| Mechanism | Source | Use |
|---|---|---|
| `template_id` pattern `*.v<n>` | `template.schema.json:11-15` | Immutable identity per major template generation |
| `version` integer | `template.schema.json:24` | Monotonic within logical family |
| `inherits_from` | `template.schema.json:42` | Composition; parent frozen at resolve time |
| `deprecated_by` | `template.schema.json:44` | Supersession pointer |
| `engine_compatibility` | `template.schema.json:32-40` | `min_version`, `max_version`, `verified_on[]` |
| `expected_revision` | ADR-0006 | Template store optimistic concurrency |
| `content_hash` | ADR-0004/0006 | Hash of canonical template JSON (sorted keys) |

**Engine upgrade policy:** templates with `engine_compatibility.verified_on` not
including current engine version → `created_with_warnings` on instantiate, not
hard-fail. Templates referencing removed APIs → `known_failure_cases` documents fix.

**Project overrides:** store at `/Game/__UeremcpTemplates/overrides/<template_id>.json`
(project layer) merging over repo `templates/` (shared layer). Resolution order:
project override → repo template → `inherits_from` chain. Project wins on field
conflict.

---

## Elemental variation model (fire / water / wind / earth)

**Design principle:** one parameterized pattern, not four primitive tools.

### Canonical template

`niagara.projectile.elemental.v1` with:

```json
{
  "inputs": {
    "type": "object",
    "required": ["element"],
    "properties": {
      "element": { "enum": ["fire", "water", "wind", "earth"] },
      "scale": { "type": "number" },
      "intensity": { "type": "number" },
      "colour_primary": { "type": "string" }
    }
  },
  "supported_modifiers": [
    "crystalline_fragments",
    "reduce_trail_persistence",
    "add_impact_decal",
    "preserve_networking",
    "preserve_damage"
  ]
}
```

### Element presets (internal, not separate tools)

| Element | Material features | Niagara behaviour | Typical ranges |
|---|---|---|---|
| fire | radial_falloff, animated_noise, warm palette | turbulence, ribbon trail | intensity 0.7–1.0 |
| water | flow_noise, caustic shimmer, cool palette | fluid drag, droplet burst | intensity 0.5–0.9 |
| wind | soft alpha erosion | velocity-aligned streaks, low drag | scale 0.8–1.5 |
| earth | rocky erosion, dust | heavy particles, short lifetime | scale 1.0–2.0 |

Presets live in template `quality_tiers` or an `element_presets` block (proposal to
WS-01 for schema extension if needed — **extend `specification` only** per schema
rule; element presets fit inside existing `typical_ranges` + `construction_plan`
conditionals).

### Modifier grammar (POC C headline example)

```
instantiate_template({
  template_id: "niagara.projectile.elemental.v1",
  inputs: { element: "water" },
  modifiers: {
    replace: { turbulence: "crystalline_fragments" },
    adjust: { trail_persistence: -0.4 },
    add: { impact_decal: "frost_decal" },
    preserve: ["networking", "damage"]
  }
})
```

**One operation** — modifiers compile into `construction_plan` step patches before
`execute_plan`. `preserve` steps generate validation-only checks, not mutations.

**Negative finding:** `modifiers` object shape is **not yet in frozen schema** — needs
WS-01 approval via `schemas/domains/project/instantiate_template.schema.json` (proposal
in `ws-15-adr-0008-recommendation.md`).

---

## Security of agent-authored templates

| Threat | Mitigation |
|---|---|
| Agent writes malicious `construction_plan` | Plans validated against `batch/plan.schema.json`; action allowlist in executor (WS-05/12) |
| Agent floods template store | Allowed roots: `/Game/__UeremcpTemplates/agent/` only for runtime writes; repo `templates/` human PR only |
| Agent overwrites shipped templates | Core templates loaded read-only from plugin; `UpdateSkill`-style refusal for native/repo paths |
| Agent promotes broken asset as template | `promote_to_template` defaults `dry_run: true`; requires validation pass before commit |
| Agent discovers internal templates | Path filter mirroring `AgentSkillBlockedNames` — hide `/UeremcpTemplates/internal/` |
| Unbounded plan size | `limits` from REAgentTools precedent — max operations per plan `[VERIFIED: REAgentTools common/limits.py — pattern]` |
| Provenance | `authored_by: "agent"` + audit log entry (WS-12) |

Epic's explicit permission gate on `CreateSkill`/`UpdateSkill`
`[VERIFIED: AgentSkill.h:82-83]` sets precedent: **`promote_to_template` and
agent template commits require `options.confirm: true` or user tier** (coordinate
WS-12 ADR-0010).

Quarantine flow: agent-authored templates land in `agent/quarantine/` until
`validate_template` passes; only then movable to `agent/approved/` or proposed for
repo inclusion.

---

## Negative findings (for WS-01 / WS-14)

1. **`UAgentSkill` cannot carry `construction_plan` without abusing `Instructions`.**
   `[VERIFIED: AgentSkill.h:17-26, 50-52]`

2. **`GetSkills` does not return subclass structured fields.**
   `[VERIFIED: AgentSkill.cpp:60-85]`

3. **`SemanticSearchToolset` does not index JSON template files.**
   `[VERIFIED: SemanticSearchToolset.h:47-48]`

4. **No Epic template versioning, inheritance, or modifier grammar.**
   `[VERIFIED: AgentSkill.h full file]`

5. **REAgentTools has no template library — no migration source.**
   `[VERIFIED: docs/audit/reagenttools.md]`

6. **POC C / third-generation proof blocked on WS-07 POC B + WS-05 `execute_plan`.**
   `[VERIFIED: docs/WORK_ALLOCATION.md Wave 2 dependencies]`

7. **`SemanticSearchToolset` not enabled in `RE.uproject`** — runtime reachability
   unconfirmed `[VERIFIED: docs/audit/epic-toolsets.md:42, 55-61]` — RB-02 must
   confirm before relying on it in integration tests.

8. **Promotion diff-minimisation unproven** until domain read APIs return complete
   structured state (ADR-0004).

---

## Implementation plan (conditional on Phase 1 gates)

### Gate dependencies

| Gate | Owner | Blocks |
|---|---|---|
| `execute_plan` executor | WS-05 | `instantiate_template` |
| POC B fireball | WS-07 | Seed template + POC C gen 2 |
| `UeremcpCore` compiling toolset | WS-03 | All C++ tools |
| Schema validator green | WS-05 | Template load |
| Permission tiers | WS-12 | Agent write paths |
| Editor test harness | WS-11 | Integration tests |

### Phase 2A — substrate (WS-15, post-gates)

1. `UeremcpTemplates` module: JSON loader, in-memory index, schema validation on load.
2. `search_templates` — metadata search only.
3. Seed `templates/niagara/niagara.projectile.elemental.v1.json` from WS-07 emitter
   archetypes (proposal when WS-07 delivers).
4. Unit tests: template load, inherit merge, input validation, modifier compilation.

### Phase 2B — execution (WS-15 + WS-07)

5. `instantiate_template` → `execute_plan` delegation.
6. POC C gen 2: ice variation from elemental template.
7. Integration test: POC C7 third generation (earth or wind).

### Phase 2C — promotion and compounding

8. `promote_to_template` with base-template diff (dry_run default).
9. `known_failure_cases` auto-append on validation failure (quarantine only).
10. Optional: companion `UAgentSkill` generator for top templates.
11. Optional: `SemanticSearchToolset` integration for asset-side discovery.

---

## Answers to brief questions

| # | Question | Answer |
|---|---|---|
| 1 | `FAgentSkillDetails` / storage / discovery | One `Instructions` string; Blueprint assets under `/Game/...`; `GetDerivedClasses` enumeration `[VERIFIED: AgentSkill.h:17-26, AgentSkill.cpp:26-57]` |
| 2 | Subclass for structured payloads? | Technically yes (`Blueprintable`), but `GetSkills` won't surface them — **not viable as primary** |
| 3 | `CreateSkill` accepts / writes? | Folder + name + description + details → Blueprint asset `[VERIFIED: AgentSkill.cpp:88-130]` |
| 4 | Search mechanism? | Epic: none for skills. `SemanticSearchToolset` for assets only. **UEREMCP builds `search_templates`** |
| 5 | Versioning/inheritance? | Epic: none. **UEREMCP: `template_id.v<n>`, `inherits_from`, ADR-0006 revisions** |
| 6 | `GeneratePrompt` complement? | Yes — optional companion skill; cheap, not substrate |
| 7 | Bespoke asset type? | **JSON canonical**; optional `UPrimaryDataAsset` later |
| 8 | Promotion feasibility? | Yes with base-template diff; quality depends on read APIs |
| 9 | Frost-from-fireball one operation? | `instantiate_template` + modifier grammar (proposed) |
| 10 | Failure recording? | Auto on quarantine templates; human/agent `update_template` for diagnosis |
| 11 | Project overrides? | `/Game/__UeremcpTemplates/overrides/` over repo `templates/` |
| 12 | Engine compatibility? | `engine_compatibility` block; warn-not-fail on version mismatch |

---

## Deliverables checklist

- [x] Recommendation for ADR-0008, with evidence
- [ ] Working store / search / instantiate / promote — **deferred post-gates (implementation)**
- [x] Modifier grammar design for headline example (see Elemental variation)
- [ ] Seed templates from WS-07 — **deferred; blocked on WS-07**
- [x] Failure-recording loop documented

---

## Audit entry (for WS-02 matrix — via proposal)

| Toolset | Tool | Disposition | Notes |
|---|---|---|---|
| `UAgentSkillToolset` | `ListSkills`, `GetSkills`, `CreateSkill`, `UpdateSkill` | **preserve** | Prompt-shaped skill CRUD; not execution templates |
| `SemanticSearchToolset` | `Search`, `FindSimilar` | **compose** | Asset similarity for promotion; not template index |

See [`docs/proposals/ws-15-audit-rows.md`](../proposals/ws-15-audit-rows.md).
