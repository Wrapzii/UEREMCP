# WS-10 proposal: register Animation and carry non-graph asset state

- **From:** WS-10
- **To:** WS-03 (plugin descriptor), WS-01/WS-05 (response contract), WS-01
  (capability catalog)
- **Date:** 2026-07-30
- **Status:** blocking agent-reachable completion; owned implementation and tests exist

## Delivered owned slice

`UeremcpAnimation` implements `inspect_montage` as one read-only semantic operation.
Its domain service emits skeleton, slots, all segments, sections, real notify/state
objects, trigger policy, dependencies, and a deterministic content hash
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpAnimation/Private/UeremcpAnimationService.cpp]`.

The notify read uses `UAnimationBlueprintLibrary::GetAnimationNotifyEvents`
`[VERIFIED: $UE_ROOT/Engine/Source/Editor/AnimationBlueprintLibrary/Public/AnimationBlueprintLibrary.h:230-232]`.
Slot, segment, and section fields are public
`[VERIFIED: Engine/Source/Runtime/Engine/Classes/Animation/AnimMontage.h:37-93,695-749]`
`[VERIFIED: Engine/Source/Runtime/Engine/Classes/Animation/AnimCompositeBase.h:65-138]`.

`read_anim_bp` inventory scaffold is also owned-implemented: graph enumeration via
`GetAnimationGraphs` / `GetAllGraphs`, type discriminators, node counts, and fidelity
flags with `nodes_emitted=false`. Full ADR-0004 node/link emission awaits WS-06
shared EdGraph serialization — ask recorded in
`docs/proposals/ws-10-edgraph-share-ws06.md`. Tool response remains
`partially_completed` and does not emit `asset_state` while ADR-0011 is Proposed.
`[VERIFIED: AnimationBlueprintLibrary.h:681]`
`[VERIFIED: Blueprint.h:1107]`

## Blocker 1 — plugin descriptor registration (WS-03)

`UEREMCP.uplugin` does not list `UeremcpAnimation`, so UnrealBuildTool will not build
or load the owned module `[VERIFIED: Plugins/UEREMCP/UEREMCP.uplugin:14-62]`.
WS-10 does not own that descriptor.

Requested insertion in `Modules`:

```json
{
  "Name": "UeremcpAnimation",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": ["Editor"]
}
```

No new plugin dependency is required: `AnimationBlueprintLibrary` is an engine module,
and the owned `UeremcpAnimation.Build.cs` declares it directly
`[VERIFIED: Engine/Source/Editor/AnimationBlueprintLibrary/AnimationBlueprintLibrary.build.cs]`.

After integration, run:

1. Build `REEditor Win64 Development -NoHotReloadFromIDE`.
2. Run `UEREMCP.Animation.*`.
3. Confirm `UeremcpAnimation.UeremcpAnimationToolset` appears in `list_toolsets`.
4. Confirm `describe_toolset` exposes `InspectMontage` and `ReadAnimBp`.

## Blocker 2 — frozen response has no structured non-graph state (WS-01/WS-05)

The request permits domain extension only under `specification`
`[VERIFIED: docs/adr/ADR-0003-request-response-envelope.md:25-39]`. The response root
and `result` both set `additionalProperties: false`; `result` carries only asset/change
references and operation steps
`[VERIFIED: schemas/envelope/response.schema.json:8,33-71]`. `diagnostics.graphs` is
restricted to `graph.schema.json`
`[VERIFIED: schemas/envelope/response.schema.json:109-140]`. A montage is not a graph.

Therefore there is currently no conformant place for the complete montage state.
Encoding it into `summary`, a diagnostic message, or a fake graph would violate the
typed contract and the complete-state objective. The tool currently returns
`partially_completed`, counts, dependencies, and revision while withholding the
structured object.

Requested contract decision: add this optional result property:

```json
"asset_state": {
  "description": "Complete structured state for a non-graph primary asset. Its shape is selected by action and defined by the owning domain response schema.",
  "type": "object"
}
```

Then add action-selected response schemas under each domain (starting with
`schemas/domains/animation/inspect_montage.response.schema.json`) and update
`FUeremcpResponse` / serialization to carry `AssetState`. This is preferable to an
untyped JSON string and preserves one response envelope.

If WS-01 rejects an inline field, the accepted alternative must add a schema-valid
`resource_link` location; ADR-0009 allows large complete payloads as MCP resources,
but the current response schema has no `resource_link`
`[VERIFIED: docs/adr/ADR-0009-long-running-jobs.md:60-63]`
`[VERIFIED: schemas/envelope/response.schema.json]`.

## Blocker 3 — capability catalog (WS-01)

Add actions, not primitives:

- Domain: `animation`
- Action: `inspect_montage`
- Mode: read-only
- Specification: `schemas/domains/animation/inspect_montage.schema.json`
- Result: complete montage adjacent state after Blocker 2
- Verification: asset class load, all public arrays enumerated, canonical content hash
- Limitations: no montage mutation; Control Rig continues to compose Epic
  AnimationAssistant primitives

- Domain: `animation`
- Action: `read_anim_bp`
- Mode: read-only inventory
- Specification: `schemas/domains/animation/read_anim_bp.schema.json`
- Result: graph inventory + revision now; full ADR-0004 node/link state after
  WS-06 shared EdGraph serializer (see `ws-10-edgraph-share-ws06.md`) and after
  Blocker 2 if emitted via `asset_state` / `diagnostics.graphs`
- Verification: AnimBP load, `GetAnimationGraphs` + `GetAllGraphs`, content hash
- Limitations: no AnimBP state-machine authoring; nodes/links not yet emitted

## Blocker 4 — shared EdGraph reader (WS-06)

Full AnimBP node/link dump cannot land on a forked walker. Proposal:
`docs/proposals/ws-10-edgraph-share-ws06.md`. Meanwhile inventory-only
`partially_completed` remains the honest agent surface.

## Completion boundary

WS-10 can claim the domain service is implemented and source-tested. It cannot claim
the agent-facing operation is registered, runtime-tested, or complete until Blockers
1 and 2 are integrated. `partially_completed` is intentional, not a placeholder
success.
