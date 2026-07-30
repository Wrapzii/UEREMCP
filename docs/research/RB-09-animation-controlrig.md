# RB-09: Animation Blueprints, state machines, Control Rig, IK, retargeting

- **Owner:** WS-10
- **Status:** complete
- **Blocks:** Wave 3 design; ADR-0004 coverage for animation graphs
- **Priority:** medium — but **design work happens now, not later**
- **Last updated:** 2026-07-29

## Framing

Master prompt §9 is explicit: animation, rigging, and locomotion must be in the
architecture now, even if the first implementation is thinner than Niagara or
Blueprint. **Do not defer these domains to an undefined future system.**

The realistic expectation going in was that Control Rig and AnimBP state machines
have the weakest public authoring APIs of any domain in scope. Source + runtime
evidence revises that: **Control Rig authoring is real and already shipped by Epic**;
**AnimBP structure is readable but Blueprint DSL does not capture it**; **goal-level
AnimBP state-machine authoring remains unproven and must not be promised**.

`AnimationAssistantToolset` and `SequencerAnimMixerToolset` are enabled in the RE
project `[VERIFIED: RE.uproject]`. Runtime `list_toolsets` confirmed AnimationAssistant
classes loaded `[VERIFIED-RUNTIME: list_toolsets 2026-07-29]`.

## Questions

(Unchanged from assignment — answered in Findings.)

### A. Animation Blueprints
### B. Control Rig
### C. Animation assets
### D. Integration

## Findings

### Audit — AnimationAssistantToolset (do not rebuild)

**320 Python tools** across 9 toolset classes; 0 C++ tools
`[VERIFIED: UEREMCP-ws02/docs/audit/raw/plugins/AnimationAssistantToolset.json]`.

| Class | Tools | Role |
|---|---:|---|
| `SequencerTools` | 140 | LevelSequence CRUD, bindings, tracks, sections, playback |
| `SequencerControlRigTools` | 72 | Control Rig keying/baking/layers/spaces in Sequencer |
| `ControlRigTools` | 44 | Control Rig asset create, hierarchy, RigVM graph nodes/pins |
| `SequencerKeyframingTools` | 22 | Channel keys / curve editor |
| `SequencerOutlinerTools` | 18 | Outliner mute/solo/lock |
| `SequencerConditionTools` | 9 | Track/section conditions |
| `SequencerCustomBindingTools` | 8 | Possessable/spawnable/custom bindings |
| `SequencerImportExportTools` | 6 | FBX / AnimSequence export-import / link |
| (unknown/test) | 1 | — |

**Zero** tools mention AnimBlueprint, AnimGraph, StateMachine, AnimMontage,
AnimSequence factories, BlendSpace, or AnimNotify authoring
`[VERIFIED: AnimationAssistant Python scan — 0 hits]`.

Disposition for UEREMCP: **preserve Epic primitives; do not wrap 1:1**. Goal-level
actions may *compose* them (especially Sequencer→AnimSequence bake and Control Rig
pose keying) but must not republish 320 primitives to the agent.

Also preserve (not AnimationAssistant, but adjacent):

- `SkeletalMeshTools` bone/socket CRUD `[VERIFIED: skeletal_mesh.py:148,314,354]`
- `REAnimWorkflowTools` pose→sequence→montage composite `[VERIFIED: anim_workflow_tools.py]`
- `RECharacterWorkflowTools` montage wiring + socket list `[VERIFIED: character_workflow_tools.py]`

### A1. AnimBP `AnimGraph` → `graph.schema.json` (`AnimBlueprintGraph`)

**Read: yes (structure).** `UAnimationGraph` extends `UEdGraph`
`[VERIFIED: AnimationGraph.h:20]`. `UAnimationBlueprintLibrary::GetAnimationGraphs`
enumerates them `[VERIFIED: AnimationBlueprintLibrary.h:681]`.

**Runtime:** `BlueprintTools.list_graphs` on `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`
returned `AnimGraph`, nested state-machine graphs, per-state bound graphs, transition
graphs, and `EventGraph`
`[VERIFIED-RUNTIME: BlueprintTools.list_graphs on ABP_Unarmed]`.

**DSL read: no.** `BlueprintTools.read_graph_dsl` on both `AnimGraph` and the
`Locomotion` state machine returned empty string `""`
`[VERIFIED-RUNTIME: read_graph_dsl AnimGraph + Locomotion → ""]`.

**Implication:** ADR-0004 mapping is feasible by walking `UEdGraph` nodes/pins
(share code with WS-06), **not** by reusing Blueprint DSL. Anim nodes are
`UAnimGraphNode_*`, not K2 nodes — semantic_type mapping must be AnimGraph-specific.

**Author: unproven / do not claim.** No AnimationAssistant AnimBP tools. Schema
spawners exist for state machines (below) but whole-graph `replace` is not evidenced.

### A2. State machines (`AnimStateMachine`)

**Read: yes.** `UAnimationStateMachineGraph : UEdGraph` with `EntryNode`
`[VERIFIED: AnimationStateMachineGraph.h:16-22]`. States (`UAnimStateNode`) and
transitions (`UAnimStateTransitionNode`) carry bound graphs, crossfade, blend mode,
auto-rule timing `[VERIFIED: AnimStateNode.h:29-48; AnimStateTransitionNode.h:24-100]`.

**Runtime hierarchy** from `list_graphs` includes e.g. `Locomotion`, `Main States`,
`Idle`, `Walk / Run`, `Jump`, `Fall Loop`, `Land`, and multiple `Transition` graphs
`[VERIFIED-RUNTIME: ABP_Unarmed list_graphs]`.

**Author: weak / treat as read-only for agent surface.** Schema actions
`FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate` exist
`[VERIFIED: AnimationStateMachineSchema.h:18-64]` — editor-schema style, not a
goal-level public library. No Epic toolset exposes them. **Do not promise AnimBP
state-machine authoring in Phase 4.**

### A3. Blend spaces, aim offsets, layered blend, additive, slots, sync markers

| Asset / feature | Read | Author | Evidence |
|---|---|---|---|
| BlendSpace / AimOffset assets | yes (asset props) | factory create | `UBlendSpaceFactoryNew`, `UAimOffsetBlendSpaceFactoryNew` `[VERIFIED: UnrealEd Factories]` |
| AnimGraph blend/additive/slot **nodes** | structure via UEdGraph | unproven | `AnimGraphNode_BlendSpace*`, `ApplyAdditive`, etc. exist as editor nodes |
| Sync markers on sequences | yes | yes | `Get/Add/RemoveAnimationSyncMarker*` `[VERIFIED: AnimationBlueprintLibrary.h:202-226]` |
| Montage slots / sections | yes (partial via ObjectTools) | factory + props | Runtime: `slotAnimTracks` readable on `MM_Pistol_Fire_Montage` `[VERIFIED-RUNTIME]` |

Project already has `BS_Idle_Walk_Run`, `AO_Pistol`, `AO_Rifle`
`[VERIFIED-RUNTIME: AssetTools.find_assets Mannequins]`.

### A4. AnimBP variables and EventGraph

`list_graphs` returns `EventGraph` alongside `AnimGraph`
`[VERIFIED-RUNTIME: ABP_Unarmed]`. Event graph is a normal Blueprint graph — WS-06
machinery applies. AnimBP variables are Blueprint variables on `UAnimBlueprint`
(same `list_variables` path as BP) — coordinate with WS-06; do not fork.

### B5–B6. Control Rig readability and authoring

**Ceiling: authorable (not read-only).** Epic `ControlRigTools` create / hierarchy /
RigVM node+pin ops wrap `URigVMController` + hierarchy controller
`[VERIFIED: controlrig.py:51-71,638-681; RigVMController.h:364,400,1042]`.

**Runtime (all under `/Game/__UeremcpTests/`):**

1. `ControlRigTools.create` → `/Game/__UeremcpTests/Animation/CR_WS10_Probe`
   `[VERIFIED-RUNTIME]`
2. `list_graphs` → `RigVMModel` `[VERIFIED-RUNTIME]`
3. `create_node(RigUnit_BeginExecution)` → node ref returned; `list_nodes` confirms
   `[VERIFIED-RUNTIME]`
4. `import_bones_from_asset(SKM_Manny_Simple)` → 160+ bones including `weapon_r`,
   IK helpers `[VERIFIED-RUNTIME]`
5. Production `CR_Mannequin_Body` exposes large RigVM + function-library graph tree
   `[VERIFIED-RUNTIME: list_graphs]`

**ADR-0004 fit for `ControlRigGraph`:** RigVM is **not** `UEdGraph`. Nodes/links can
still map into the shared schema, but pin typing, contained graphs, and function
library need `extensions.control_rig`. Round-trip is plausible via `URigVMController`
(Epic already does it) — fidelity of *semantic* JSON still unproven; set
`fidelity.round_trip_supported=false` until retrieve→replace→retrieve passes.

### B7–B8. IK Rig / IK Retargeter / Full Body IK

**Programmatically creatable/configurable** via public BlueprintCallable controllers:

- `UIKRigController::AddSolver` accepts solver type string including Full Body IK
  path comment `/Script/IKRig.FullBodyIKSolver`
  `[VERIFIED: IKRigController.h:97-99,151]`
- `FIKRigFBIKGoalSettings` / Full Body IK solver structs
  `[VERIFIED: IKRigFullBodyIK.h:14+]`
- `UIKRetargeterController::AutoMapChains`, `SetSourceChain`, set IK Rig refs
  `[VERIFIED: IKRetargeterController.h:70-115,285-293]`

No AnimationAssistant coverage — justified UEREMCP goal ops if needed, but defer
behind montage/notify/read paths.

### C9. Sequences, montages, notifies, curves, root motion

| Capability | Ceiling | Evidence |
|---|---|---|
| Create AnimSequence / AnimMontage | **author** | Factories + REAgentTools `[VERIFIED: AnimSequenceFactory.h; AnimMontageFactory.h; anim_helpers.py:294-318]` |
| Sequencer → AnimSequence bake | **author** | `ControlRigSequencerLibrary.export_anim_sequence_from_sequencer` `[VERIFIED: anim_helpers.py:337]` + Epic `SequencerImportExportTools` |
| Control Rig pose timeline → montage | **author (composite)** | `REAnimWorkflowTools.author_clip_from_pose_timeline` `[VERIFIED: anim_workflow_tools.py:384+]` |
| Sync markers / curves / root motion flags | **author** | `UAnimationBlueprintLibrary` `[VERIFIED: AnimationBlueprintLibrary.h:174-226,335-367]` |
| Real AnimNotify / AnimNotifyState | **author via library** | `AddAnimationNotifyEvent`, `AddAnimationNotifyStateEvent` `[VERIFIED: AnimationBlueprintLibrary.h:240-269]` |
| REAgentTools "notify plan" | **metadata only — not real notifies** | `apply_notify_plan_metadata` docstring: "Full AnimNotifyState authoring via Python is engine-version fragile" `[VERIFIED: anim_helpers.py:343-347]`; runtime `create_montage_from_anim` returned `notify_meta: ["plan=impact=0.33,vfx=0.1"]` only `[VERIFIED-RUNTIME]` |
| Montage slot tracks | **read** | ObjectTools `slotAnimTracks` on `MM_Pistol_Fire_Montage` `[VERIFIED-RUNTIME]` |
| Montage `Notifies` / `CompositeSections` via ObjectTools | **neither (via that path)** | `get_properties` failed those names `[VERIFIED-RUNTIME]` — use AnimationLibrary instead |

**REAgentTools CAPABILITY_MATRIX claim verified with caveat:** pose→AnimSequence→Montage
path exists and works; **notify authoring claim is overstated** (tags/plan only).

### C10. Skeleton / sockets

- Bones: `SkeletalMeshTools.get_bone_names` / parent / children
  `[VERIFIED: skeletal_mesh.py:148+]` + runtime on `SKM_Manny_Simple`
- Sockets: `add_socket` / `get_socket_names` / transform / remove
  `[VERIFIED: skeletal_mesh.py:314+]`; runtime added `WS10_VFX_Probe` on duplicated
  `/Game/__UeremcpTests/Animation/SKM_WS10_Probe` `[VERIFIED-RUNTIME]`
- Virtual bones: `UAnimationBlueprintLibrary::AddVirtualBone` /
  `USkeleton::AddNewVirtualBone` `[VERIFIED: AnimationBlueprintLibrary.h:578; Skeleton.h:469]`

Note: `/Game/Characters/Mannequins/Meshes/SK_Mannequin` is class **Skeleton**, not
SkeletalMesh `[VERIFIED-RUNTIME: get_asset_class]`. Mesh is `SKM_Manny_Simple` /
`SKM_Quinn_Simple`.

### C11. Notifies that trigger Niagara / GAS

Public path: `AddAnimationNotifyEvent` / `AddAnimationNotifyStateEvent` with
project notify subclasses (Niagara notify, gameplay ability notify, custom).
REAgentTools does **not** do this today — UEREMCP must, and it is the WS-07 / WS-09
seam. Prefer real notifies over metadata tags.

### C12. Retargeting

`UIKRetargeterController` AutoMap + chain mapping is public
`[VERIFIED: IKRetargeterController.h]`. Comfy/FBX external path is documented by
REAgentTools as out-of-UE `[VERIFIED-RUNTIME: get_animation_pipeline_notes]`.

### D13. Ability ↔ animation contract (proposal to WS-09)

For `create_spell` / ability montage wiring, one request should carry:

```json
{
  "montage": { "path": "/Game/.../AM_...", "slot": "DefaultSlot" },
  "notifies": [
    { "name": "Impact", "time": 0.33, "class": "/Script/...AnimNotify_GameplayCue" },
    { "name": "VFX", "time": 0.1, "class": "...AnimNotify_PlayNiagaraEffect", "socket": "hand_r" }
  ],
  "sockets": [{ "name": "hand_r", "mesh": "/Game/.../SKM_..." }],
  "root_motion": false
}
```

WS-10 owns montage+notify+socket creation/validation; WS-09 owns ability asset refs
and cue/tag binding. See `docs/proposals/ws-10-ability-anim-contract.md`.

### D14. Networked animation

Worth validating later (not blocking research): montage replication to simulated
proxies, root-motion authority (`bEnableRootMotion` / montage root motion), and
notify execution on server vs client. No runtime net test run in this brief —
record as open.

### Compile / validation

- AnimBP: `BlueprintTools.compile_blueprint` applies (AnimBP is a Blueprint subclass).
  Always re-`list_graphs` / inspect diagnostics after write; never claim
  `*_validated` without it (`AGENTS.md` rule 6).
- Control Rig: compile via Control Rig blueprint compile path (Epic tools save after
  create); Phase 4 must await compile + re-read RigVM nodes.
- Sequences/montages: save + re-read notifies/slots/length.

### Stable graph mapping

| `graph_type` | Engine type | Stable identity | Round-trip today |
|---|---|---|---|
| `AnimBlueprintGraph` | `UAnimationGraph` (`UEdGraph`) | `semantic_id` from node role + anim asset refs | read structure yes; DSL no; write unproven |
| `AnimStateMachine` | `UAnimationStateMachineGraph` | state/transition names + bound graph ids | read hierarchy yes; write unproven → **agent-facing read-only** |
| `ControlRigGraph` | `URigVMGraph` (not UEdGraph) | RigVM node names + unit struct path | read+author via Epic tools; JSON round-trip unproven |

Use `fidelity.lossy_areas` aggressively. Prefer `response_detail: summary` for large
Control Rigs (`CR_Mannequin_Body` has dozens of contained graphs).

## Capability ceiling table

| Sub-domain | Read | Author | Agent surface |
|---|---|---|---|
| AnimSequence create/edit (tracks via model) | yes | yes | goal: create/inspect |
| AnimMontage + slots | yes | yes | goal: create/inspect |
| Sync markers / curves / root-motion flags | yes | yes | include in inspect/submit |
| AnimNotify / NotifyState | yes | yes (AnimationLibrary) | **must** author real notifies |
| Skeleton bones | yes | n/a (mesh-owned) | inspect |
| Sockets / virtual bones | yes | yes | goal: ensure_socket |
| BlendSpace / AimOffset **assets** | yes | factory | low priority |
| AnimBP AnimGraph structure | yes | unproven | **read** first |
| AnimBP EventGraph | yes | via WS-06 BP path | share WS-06 |
| AnimBP state machines | yes | schema-only / unproven | **read-only** |
| Control Rig hierarchy | yes | yes (Epic) | compose; don't rebuild |
| Control Rig RigVM graph | yes | yes (Epic) | compose; `extensions.control_rig` |
| Sequencer authoring | yes | yes (Epic 200+ tools) | internal only |
| IK Rig / Full Body IK | yes | yes (controllers) | defer Phase 4+ |
| IK Retargeter | yes | yes (controllers) | defer |
| Motion warping windows | partial | plugin-specific | open |

## ADR-0004 verdict

| Type | Fits shared schema? | Notes |
|---|---|---|
| `AnimBlueprintGraph` | **Yes** | UEdGraph walk like Blueprint; different node classes; **no** Blueprint DSL reuse |
| `AnimStateMachine` | **Yes** (read) | Nested subgraphs already match `subgraphs[]`; authoring out of scope |
| `ControlRigGraph` | **Yes with extensions** | Map RigVM nodes/links; put unit struct paths, hierarchy keys, contained graphs in `extensions.control_rig`. Not a reason to fork ADR-0004 |

No ADR challenge filed — representation holds; achievability differs by family.

## Negative findings

1. **AnimationAssistant has no AnimBP / montage / notify / blend-space tools** —
   320 tools are Sequencer + Control Rig only.
2. **`BlueprintTools.read_graph_dsl` returns empty for AnimGraph and state machines** —
   DSL path is a dead end for AnimBP.
3. **REAgentTools notify path does not create AnimNotify objects** — metadata/plan
   only; library APIs that *do* exist were unused.
4. **ObjectTools cannot read montage `Notifies` / `CompositeSections` /
   `EnableRootMotion` by those names** — use AnimationLibrary.
5. **`SK_Mannequin` path is a Skeleton asset**, not a SkeletalMesh — easy agent trap.
6. **No runtime net/replication validation** performed this brief.
7. **AnimBP state-machine write** not exercised (would risk user content); treat as
   unsupported until a `__UeremcpTests` duplicate round-trip exists.
8. **Motion warping programmatic authoring** not verified beyond plugin presence.

## API availability summary

| API / capability | Public | Editor-only | C++ | Python | Notes | Tag |
|---|---|---|---|---|---|---|
| `UAnimationGraph` / state machine graphs | yes | editor module | yes | via UObject | UEdGraph subclasses | `[VERIFIED: AnimationGraph.h:20]` |
| `UAnimationBlueprintLibrary` notifies/markers/curves | yes | editor | yes | yes | ScriptName AnimationLibrary | `[VERIFIED: AnimationBlueprintLibrary.h:240]` |
| `ControlRigTools` (Epic) | yes | editor plugin | no | yes | 44 tools | `[VERIFIED-RUNTIME]` |
| `URigVMController` | yes | developer | yes | yes | add unit/link/variable | `[VERIFIED: RigVMController.h:364]` |
| `UIKRigController` / FBIK | yes | editor | yes | yes | AddSolver | `[VERIFIED: IKRigController.h:99]` |
| `UIKRetargeterController` | yes | editor | yes | yes | AutoMapChains | `[VERIFIED: IKRetargeterController.h:286]` |
| `SkeletalMeshTools` sockets | yes | editor | no | yes | preserve | `[VERIFIED-RUNTIME]` |
| `REAnimWorkflowTools` | project | editor | no | yes | reuse composite pattern | `[VERIFIED-RUNTIME]` |
| AnimBP state machine goal authoring | no Epic tool | — | schema only | — | **unsupported** | negative finding |

## Architectural implications

1. **R-08 refined (not wholesale confirmed):** Control Rig is **not** effectively
   read-only. AnimBP state machines are **effectively read-only on the agent surface**
   until a proven write path exists. Document both in `capability_notes`.
2. **Do not rebuild AnimationAssistant.** Compose Epic + REAgentTools; ship goal ops.
3. **Share UEdGraph read with WS-06** for AnimBP; add AnimGraph semantic mapping layer.
4. **Control Rig JSON** uses `extensions.control_rig`; prefer wrapping Epic tools
   internally over reimplementing RigVMController.
5. **Phase 4 implementation plan (honest):** see Implementation plan below — no
   AnimBP state-machine authoring; no Sequencer re-wrap; real notifies required.
6. **Schemas:** specification shapes proposed in
   `docs/proposals/ws-10-animation-schemas.md` — **not** creating
   `schemas/domains/animation/` until Wave 3 implementation is authorized.

## Implementation plan (research only — not authorized to build)

### Phase 4a — ship first (high value, low API risk)

1. `animation.inspect_sequence` / `inspect_montage` — complete adjacent context
   (length, slots, segments, notifies via AnimationLibrary, sync markers, root
   motion, skeleton).
2. `animation.ensure_montage` — create/update montage from sequence + **real**
   notifies + slots; verify by re-read.
3. `animation.ensure_socket` — wrap/compose `SkeletalMeshTools` under allowed roots.
4. Reuse REAgentTools pose→bake path as internal primitive for combat clip authoring
   when Sequencer bake is required.

### Phase 4b — graph read

5. `animation.read_anim_bp` — `list_graphs` + UEdGraph walk → `AnimBlueprintGraph` /
   `AnimStateMachine` JSON with `fidelity.round_trip_supported=false`.
6. Coordinate with WS-06 for shared EdGraph serializer.

### Phase 4c — Control Rig (compose, don't rebuild)

7. Goal ops that call Epic `ControlRigTools` / Sequencer bake under the hood;
   agent sees one envelope action, not 44 primitives.
8. Optional `control_rig.read_graph` → `ControlRigGraph` + extensions.

### Explicitly out of Phase 4 promises

- AnimBP state-machine **authoring** / whole-graph replace
- Re-exposing Sequencer 200+ tools
- Claiming notify support via metadata tags
- IK Retarget / Full Body IK goal ops (API exists; defer)

## capability_notes (ready to ship)

```
Animation: sequences/montages/notifies/sockets are authorable via public editor APIs.
AnimBP AnimGraph and state machines are readable as structured graphs; Blueprint DSL
does not apply. AnimBP state-machine authoring is unsupported — do not submit
replace/patch for AnimStateMachine. Control Rig hierarchy and RigVM graphs are
authorable via Epic AnimationAssistant ControlRigTools (composed internally; not
re-exposed as primitives). REAgentTools pose→AnimSequence→Montage is prior art;
UEREMCP will author real AnimNotifies (not metadata tags). Round-trip fidelity flags
are false until retrieve→replace→retrieve passes per family.
```

## Open questions

1. Can `FEdGraphSchemaAction_NewStateNode` be driven safely from C++ without UI for
   a minimal locomotion SM under `__UeremcpTests`? (Blocks any future write claim.)
2. Which project AnimNotify subclasses exist for Niagara + GAS? (Inventory with WS-07/09.)
3. AnimDataController path for raw track editing on AnimSequence in 5.8 (raw track
   getters deprecated since 5.2).
4. Montage replication / root-motion authority PIE test plan (WS-11).
5. Whether `SequencerAnimMixerToolset` adds anything AnimationAssistant lacks
   (enabled in RE.uproject; not deeply audited here).

## Wave 2 implementation update (2026-07-30)

WS-10 implemented the largest independent read-only slice, `inspect_montage`, under
`Plugins/UEREMCP/Source/UeremcpAnimation/`. One domain-service call returns skeleton,
slots, segments, sections, real notify/state objects, dependencies, and a deterministic
content hash `[VERIFIED: UeremcpAnimationService.cpp]`. It reads real notify events
through `UAnimationBlueprintLibrary::GetAnimationNotifyEvents`
`[VERIFIED: AnimationBlueprintLibrary.h:230-232]`; it does not use REAgentTools'
metadata-only notify plan.

The tool boundary remains honestly `partially_completed`: the frozen response schema
has no structured field for complete non-graph asset state
`[VERIFIED: schemas/envelope/response.schema.json:33-71,109-140]`. The original
integration requests are recorded in
`docs/proposals/ws-10-animation-integration-blockers.md`.

Follow-up after orchestration integration: `UeremcpAnimation` is now listed in the
plugin descriptor `[VERIFIED: Plugins/UEREMCP/UEREMCP.uplugin:40-44]`. WS-10 authored
`UEREMCP.Animation.InspectMontage.EditorScratchAsset`, which creates a GUID-unique,
in-memory montage package under `/Game/__UeremcpTests/Animation/`, calls the real
tool boundary with the package path, asserts the honest partial response and
validation evidence, and never saves the fixture
`[VERIFIED: UeremcpAnimationTests.cpp]`. This test is **authored, not runtime-passed**;
the separate editor lane must execute it before any runtime claim.

WS-01 drafted the future typed `result.asset_state` amendment as **Proposed
ADR-0011**, not Accepted, and left the frozen envelope unchanged
`[VERIFIED: ws-01-orch/docs/adr/ADR-0011-non-graph-asset-state.md:1-32]`.
The action-owned emitted-state contract now exists at
`schemas/domains/animation/inspect_montage.asset-state.schema.json`; the tool remains
`partially_completed` until the protocol amendment and two-pass validator land.
The editor fixture submits both package and full object paths and requires one
canonical package identity and revision
`[VERIFIED: UeremcpAnimationTests.cpp; PackageName.h:882-888]`.
Notify state now includes `track_index` and is canonicalized with the engine's own
ordering rule (trigger time, then track index)
`[VERIFIED: AnimTypes.h:458-474; UeremcpAnimationService.cpp]`. The authored
`NotifyOrdering` automation test covers reversed raw storage, an invalid track index,
and a notify with no object class; it is not a runtime-pass claim.

Mutation (`ensure_montage`) remains blocked behind shared mutator-queue / sandbox
orchestration: `FUeremcpMutatorQueue::IsImplemented()` returns false
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpMutatorQueue.cpp:3-18]`.
Implementing an unsandboxed montage write would contradict ADR-0005 and ADR-0010.

## Orch compile and runtime verification (2026-07-30)

The RE plugin junction resolved to
`$UEREMCP_ROOT-ws01\Plugins\UEREMCP` on
`ws-01-orch` before verification
`[VERIFIED-RUNTIME: PowerShell Get-Item -Force reported LinkType=Junction and the
UEREMCP-ws01 target]`.

The first isolated `UeremcpAnimation` build exposed an incomplete-type conversion
for `UAnimNotifyState`. Adding its defining public header is grounded by the engine
declaration `[VERIFIED: Engine/Source/Runtime/Engine/Classes/Animation/AnimNotifies/AnimNotifyState.h:34]`.
The subsequent `REEditor Win64 Development -Module=UeremcpAnimation
-NoHotReloadFromIDE -WaitMutex` build compiled and linked
`UnrealEditor-UeremcpAnimation.dll` with `Result: Succeeded`
`[VERIFIED-RUNTIME: UnrealBuildTool module build on 2026-07-30 at orch tip 912a89d]`.

`UEREMCP.Animation` automation was then attempted through `UnrealEditor-Cmd`.
Startup stopped before `UeremcpAnimation` loaded because the integrated plugin's
`UeremcpMaterial` binary was missing; no Animation test executed, so there is no
runtime validation claim
`[VERIFIED-RUNTIME: RE/Saved/Logs/RE.log reported "module 'UeremcpMaterial' could
not be found" before the Animation module load/test filter]`. Material/Niagara and
full-RE build/editor owners subsequently occupied the exclusive lane, so the smoke
status is `partially_completed`, not validated. Re-run
`Automation RunTests UEREMCP.Animation` when the integrated plugin binary set is
complete and the lane is free.

The offline animation contract suite passed all 12 tests, and the repository schema
validator passed all 21 schemas with resolved references and valid examples
`[VERIFIED-RUNTIME: python test_animation_contract.py and
python tools/validate_schemas.py on 2026-07-30]`.

## Deliverables checklist

- [x] Capability ceiling table
- [x] ADR-0004 fit verdict for AnimBlueprintGraph / AnimStateMachine / ControlRigGraph
- [x] `schemas/domains/animation/inspect_montage.schema.json`
- [x] `schemas/domains/animation/inspect_montage.asset-state.schema.json`
- [ ] Remaining animation schemas — deferred until their implementations start
- [x] Animation↔ability contract drafted for WS-09 (`docs/proposals/ws-10-ability-anim-contract.md`)
- [x] `capability_notes` text
- [x] Runtime probes confined to `/Game/__UeremcpTests/`
- [x] R-08 verdict recorded
