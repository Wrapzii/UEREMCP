# RB-09: Animation Blueprints, state machines, Control Rig, IK, retargeting

- **Owner:** WS-10
- **Status:** not_started
- **Blocks:** Wave 3 design; ADR-0004 coverage for animation graphs
- **Priority:** medium — but **design work happens now, not later**

## Framing

Master prompt §9 is explicit: animation, rigging, and locomotion must be in the
architecture now, even if the first implementation is thinner than Niagara or
Blueprint. **Do not defer these domains to an undefined future system.**

The realistic expectation is that Control Rig and AnimBP state machines have the
weakest public authoring APIs of any domain in scope. Establishing *where the ceiling
is* — honestly and early — is this brief's main value, so ADR-0004 and the roadmap
reflect reality rather than hope.

`AnimationAssistantToolset` and `SequencerAnimMixerToolset` are enabled in the RE
project `[VERIFIED: RE.uproject]`. Start from RB-02's inventory of what they do.

## Questions

### A. Animation Blueprints

1. Can an AnimBP's `AnimGraph` be read into `graph.schema.json`
   (`graph_type: AnimBlueprintGraph`)? It is a `UEdGraph`, so RB-05's machinery may
   largely apply — confirm, and coordinate with WS-06 to share code rather than fork it.
2. Can state machines be read — states, transitions, transition rules, conduits,
   entry state — as `AnimStateMachine`? Can they be **authored**?
3. Blend spaces, aim offsets, layered blend, additive, slots, sync markers: readable?
   creatable?
4. How are AnimBP variables and the event graph (as distinct from the anim graph)
   handled?

### B. Control Rig

5. What is readable from a Control Rig asset — the rig hierarchy, controls, and the
   RigVM graph itself? The RigVM is a different graph system from `UEdGraph`; determine
   whether it fits ADR-0004 at all, or needs `extensions.control_rig` to carry it.
6. Is Control Rig graph **authoring** available via public API, or effectively
   editor-UI-only? **If it is read-only, say so plainly and early** — it changes the
   roadmap and must appear in `capability_notes` on every relevant response.
7. IK Rig / IK Retargeter: what is programmatically creatable and configurable?
8. Full Body IK setup?

### C. Animation assets

9. Animation sequences, montages, sections, slots, notifies, sync markers, curves,
   root motion, motion warping — what is creatable and editable? REAgentTools claims a
   working Control-Rig-pose-timeline → AnimSequence → Montage path
   `[UNVERIFIED — from $RAT/Docs/CAPABILITY_MATRIX.md]`. **Verify it and reuse it; that
   is real prior art.**
10. Skeleton inspection: bone hierarchy, sockets, virtual bones. Socket creation and
    attachment — needed for VFX attachment (RB-07) and ability montages (RB-12).
11. Animation notifies that trigger Niagara or gameplay ability events — how are they
    added programmatically? This is the integration seam between WS-07, WS-09, and WS-10.
12. Retargeting between skeletons: programmatic?

### D. Integration

13. What does an ability need from animation — montage reference, notify timing, socket
    for VFX attachment? Define the contract with WS-09 so `create_spell` can wire
    animation without a second request.
14. Networked animation concerns worth validating (montage replication, root motion
    authority).

## Deliverables

- [ ] An honest capability ceiling table per sub-domain: **read / author / neither**
- [ ] A verdict on ADR-0004 fit for `AnimBlueprintGraph`, `AnimStateMachine`,
      `ControlRigGraph` — including "does not fit, here is why" if that is the answer
- [ ] `schemas/domains/animation/` specification schemas for what *is* achievable
- [ ] The animation↔ability contract, agreed with WS-09
- [ ] `capability_notes` text for the limitations, ready to ship in real responses
