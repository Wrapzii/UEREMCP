# WS-13: Tool-selection discoverability (2026-07-30)

**Owner:** WS-13  
**Branch / worktree:** `ws-13-tool-selection-discoverability` @
`$UEREMCP_ROOT-ws13-tool-selection`  
**Base tip:** `645302c` (WS-13 guides on `dae0e5c`; merge-base with local `main`
`a2bd21b`)  
**Do not push.** Do not retarget RE junction (concurrent crash-fix / visual-capture
deploy).

## Goal

Make UEREMCP tool selection obvious and reliably preferred by **fresh** agents
without repository-history context or hidden prompts. Prioritization must come from
accurate names, descriptions, schemas, examples, and a published routing contract.
**Limitation:** this cannot guarantee arbitrary agent/LLM behavior.

## Inventory (static + handoff)

### Live / intended UEREMCP AICallable surface

| MCP toolset | Tools | Actions | Status (catalog / handoff) |
|---|---|---|---|
| `UeremcpCore.UeremcpReferenceToolset` | Ping, Echo, GetJobResult, CancelJob | ping, echo, get_job_result, cancel_job | available / partial |
| `UeremcpBlueprint.UeremcpBlueprintToolset` | Ping, Echo, ReadGraph, SubmitGraph | ping, echo, read_graph, submit_graph | available / partial |
| `UeremcpNiagara.UeremcpNiagaraToolset` | Echo, InspectSystem, CreateNiagaraEffect | echo, inspect_system, create_niagara_effect | available / partial |
| `UeremcpMaterial.UeremcpMaterialToolset` | Echo, CreateVfxMaterial, CreateProceduralTexture | echo, create_vfx_material, create_procedural_texture | available / partial |
| `UeremcpGameplay.UeremcpGameplayToolset` | CreateSpell | create_spell | partial (preflight only) |
| `UeremcpAnimation.UeremcpAnimationToolset` | InspectMontage, ReadAnimBp | inspect_montage, read_anim_bp | partial |
| `UeremcpTemplates.UeremcpTemplatesToolset` | SearchTemplates, InstantiateTemplate, PromoteToTemplate | search_*, instantiate_*, promote_* | partial |
| `UeremcpValidation.UeremcpVisualCaptureToolset` | CaptureEffectFrames | capture_effect_frames | **pending catalog** — WS-11 validated handoff |

`execute_plan`: catalog **partial internal** — **not** agent-facing AICallable.
Templates / tests call the interpreter (ADR-0008).

### Overlapping Epic surfaces (do not confuse)

| Epic | Disposition for agent goals covered by UEREMCP |
|---|---|
| `editor_toolset.toolsets.blueprint.BlueprintTools` | preserve primitives; **supersede agent surface** with ReadGraph/SubmitGraph |
| NiagaraToolsets.* | preserve / internalise; prefer CreateNiagaraEffect / InspectSystem |
| MaterialTools | internalise; prefer CreateVfxMaterial |
| GASToolsets | gap for full spell mutation; CreateSpell is preflight only |

### CaptureEffectFrames handoff (inspected, not merged here)

From `UEREMCP-niagara-visual-tool-validation` /
`docs/proposals/ws-11-visual-capture-catalog-schema-handoff.md`:

- Toolset `UeremcpValidation.UeremcpVisualCaptureToolset`, tool `CaptureEffectFrames`
- Live evidence cited there: IceWall control rendered pixel deltas; cold-start FAIL
- WS-13 inventoried it in `tool-selection-contract.json` + guide examples; WS-01
  catalog/schema still pending per that handoff

### `SetNameFilters`

- API present: `FToolset::SetNameFilters(BlockPatterns, AllowPatterns)`
  `[VERIFIED: Engine/Plugins/Experimental/ToolsetRegistry/.../Toolset.h:59-60]`
  `[VERIFIED: .../Toolset.cpp:59-79]`
- Authorized by ADR-0002 to hide **internal** primitives from agents
- **UEREMCP calls today: none** — `apply_now: false` in the contract
- **Do not** globally hide Epic read-only discovery. Selective mutation-primitive
  hiding is a WS-03 + domain follow-up after deploy stabilizes

## Exact policy (published)

Canonical human text: `docs/guide/tool-selection-policy.md`  
Canonical machine artifact: `docs/guide/tool-selection-contract.json`

1. Prefer UEREMCP goal-level semantic ops for create/modify/validate.
2. Use Epic for read-only discovery or catalog gaps (`planned`/`research`).
3. Avoid inspect→mutate→inspect primitive loops (WHY cost model).
4. `execute_plan` → use `InstantiateTemplate` / domain tools (not invent ExecutePlan).
5. Templates vs direct domain: library match → templates; novel one-off → domain.
6. Blueprint modify: ReadGraph → SubmitGraph with `expected_revision`.
7. Jobs: GetJobResult / CancelJob (not MCP cancel alone).
8. Visual: CaptureEffectFrames for pixel evidence only — does not author.

## Before / after matrix

| Surface | Before | After (this commit) |
|---|---|---|
| Fresh-agent entry | Guides existed; routing buried | `tool-selection-policy.md` + README/agent-usage §0 |
| Machine routing | None | `tool-selection-contract.json` + 12-intent benchmark |
| Examples | Scattered fixtures | `docs/guide/examples/{minimal,complete}/` |
| CaptureEffectFrames | Docs absent on tip | Inventoried + examples; pending catalog noted |
| CAPABILITY_CATALOG routing table | No routing section | **Proposed** below (WS-01 owns file) |
| Live `describe_toolset` comments | Uneven; templates thin | **Proposed** description patches (domain WSs) |
| Error next-tool hints | Status table only | Troubleshooting next-tool column |
| SetNameFilters | Documented in ADR/facts; unused | Verified; explicitly not applied globally |
| Automated regression | `check_guide_links.py` only | + `check_tool_selection_contract.py` |

## Where fresh agents see it

| Location | Owner | Status |
|---|---|---|
| `docs/guide/tool-selection-policy.md` | WS-13 | **Done** |
| `docs/guide/tool-selection-contract.json` | WS-13 | **Done** |
| `docs/guide/agent-usage.md` §0 | WS-13 | **Done** |
| `docs/guide/examples/**` | WS-13 | **Done** |
| `docs/CAPABILITY_CATALOG.md` routing section | WS-01 | Proposed |
| `AGENTS.md` / `CLAUDE.md` read-order pointer | WS-01 | Proposed |
| C++ UFUNCTION / class comments → `describe_toolset` | Domain WSs | Proposed |
| Runtime error strings with next-tool | Domain WSs | Proposed patterns |

## Proposed owned-path patches for other workstreams

### WS-01 — `docs/CAPABILITY_CATALOG.md` (insert under Discovery)

```markdown
## Agent routing (prefer UEREMCP)

Canonical policy: `docs/guide/tool-selection-policy.md` /
`docs/guide/tool-selection-contract.json`.

| Intent | Prefer (MCP tool) | Action | Avoid |
|---|---|---|---|
| Blueprint complete read | UeremcpBlueprint…ReadGraph | read_graph | BlueprintTools pin loops |
| Blueprint replace | …SubmitGraph | submit_graph | create_node / write_graph_dsl |
| Niagara create | UeremcpNiagara…CreateNiagaraEffect | create_niagara_effect | NiagaraToolsets primitives |
| Niagara inspect | …InspectSystem | inspect_system | ad-hoc topology scrapes |
| VFX material | UeremcpMaterial…CreateVfxMaterial | create_vfx_material | MaterialTools expressions |
| Template instantiate | UeremcpTemplates…InstantiateTemplate | instantiate_template | fictional ExecutePlan |
| Job poll/cancel | UeremcpReference…GetJobResult/CancelJob | get_job_result / cancel_job | MCP cancel alone |
| Visual frames | UeremcpValidation…CaptureEffectFrames | capture_effect_frames | screenshot authoring |

Epic tools remain appropriate for read-only discovery and catalog gaps.
```

Also add `capture_effect_frames` row when WS-11 consolidation + schema land.

### WS-01 — `AGENTS.md` / `CLAUDE.md` read order

After item 0 (`docs/WHY.md`), add:

```text
0b. docs/guide/tool-selection-policy.md — when calling editor MCP tools, prefer UEREMCP
```

### Domain WSs — `describe_toolset` description contract

Each AICallable comment should state: semantic scope, mutation/destructive,
required fields, validation guarantees, idempotency/revision, output status
vocabulary, and disambiguation from Epic names. Exact cue lists live in
`tool-selection-contract.json` → `description_cues` per tool.

Priority thin comments today: Templates (`SearchTemplates` / `InstantiateTemplate`),
Reference job tools (already decent), VisualCapture (good once merged).

### WS-03 — optional `SetNameFilters`

Only after deploy stabilizes: allowlist/blocklist for **mutation** primitives that
UEREMCP composes (e.g. MaterialTools expression writes), never a global Epic
read-only disable. Settings-backed, reversible.

### Domain WSs — error next-tool guidance (when feasible)

Pattern already in guides: `rejected`+revision → re-read tool; missing ExecutePlan →
InstantiateTemplate; partially_completed+job → GetJobResult. Mirror in `summary`
strings where cheap.

## Test results (this worktree)

```text
python docs/guide/check_guide_links.py          → (re-run after fixes)
python docs/guide/check_tool_selection_contract.py
python tools/validate_schemas.py               → OK 24 schemas
python tools/check_ownership.py --ws WS-13     → OK (docs/guide + this proposal)
```

Live `list_toolsets` / `describe_toolset`: **not confirmed this session** —
Unreal MCP transport refused connection (concurrent deploy). Leave live
validation to consolidation worker; static inventory + WS-11 handoff evidence used.

## Integration instructions

1. Merge / cherry-pick this WS-13 branch into the consolidation tip after visual-capture
   lands (or accept pending_catalog for CaptureEffectFrames).
2. WS-01 applies catalog + AGENTS pointers from this proposal.
3. Domain WSs paste description cue upgrades; re-dump `describe_toolset` when editor up.
4. Fresh-agent usability worker should point clients at
   `docs/guide/tool-selection-policy.md` (and/or load the JSON contract).
5. Do not retarget RE junction from this workstream.

## Remaining ambiguity

- Live describe text may still lag until domain comment patches land.
- CaptureEffectFrames registration timing vs this tip.
- Whether selective SetNameFilters is desirable in RE (product decision; API exists).
- Fresh agents with strong Epic habit may still pick BlueprintTools — contract
  improves odds; does not eliminate the risk.
