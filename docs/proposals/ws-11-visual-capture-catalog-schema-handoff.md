# WS-11 handoff: register the live-verified visual capture action

**Owner:** WS-11  
**Recipients:** WS-01 (schema/catalog/README), WS-13 (agent guide)  
**Status:** Live verified only when renderer-warm; catalog as partial, not available.

## Capability

- Toolset: `UeremcpValidation.UeremcpVisualCaptureToolset`
- AICallable function: `CaptureEffectFrames`
- Action: `capture_effect_frames`
- Module: `UeremcpValidation`
- Scope: observe an existing Niagara system. It does not author, modify, save, or
  compile the target asset.

The operation builds a transient stage in the current editor world, steps a
force-solo Niagara component from zero to each requested age, captures an
empty-stage baseline plus a frame series, writes PNGs below
`<Project>/Saved/UEREMCP/VfxCapture/<asset>/<request-id>/`, rereads each output,
measures luminance/pixel deltas, and tears down its actors.

## Requested owned-path changes after live proof

1. WS-01: add `capture_effect_frames` to `docs/CAPABILITY_CATALOG.md` only after
   `list_toolsets`, `describe_toolset`, and an end-to-end MCP request pass.
2. WS-01: add an action-specific `specification` schema (suggested location
   `schemas/domains/validation/capture-effect-frames.schema.json`) with:
   - `frame_count`: integer, 1..64, default 8
   - `duration_seconds`: number, 0..30, default 1.5
   - `camera`: enum `front|three_quarter|side|top`, default `three_quarter`
   - `width`, `height`: integer, 64..4096, defaults 960×540
   - `additionalProperties: false`
3. WS-13: document one complete request and the output-directory convention in
   `docs/guide/**`.
4. WS-01: mention the tool in the root README only when the deployed binary is
   live-verified.

## Mandatory limitations wording

- Requires an editor world and a functioning renderer/RHI; it is not a headless
  structural validator.
- A successful pixel-delta gate proves only that the system rendered pixels
  different from the empty stage. It does not judge appearance.
- It does not prove Niagara compile validity, emitter/renderer/material
  correctness, or runtime gameplay integration. Those require the structural
  Niagara gates.
- It writes external image evidence below `Saved/UEREMCP/VfxCapture`, not a
  `/Game` asset. Responses return paths and measurements, never image bytes.
- The target is read; the project dispatch slot is still serialized because
  the operation writes files and uses a shared transient stage origin.
- Existing output is isolated by request id. No user content asset is deleted.

## Audit disposition

Epic exposes SceneCapture/render-target and Niagara component primitives, while
REAgentTools has viewport/image capture composites. Neither provides one
agent-facing operation that sets a fixed stage, deterministically re-simulates
Niagara ages, exports a whole frame series, rereads the files, and returns
numeric baseline deltas. This action composes those primitives internally and
does not expose a screenshot-driven authoring loop.

API evidence used by the implementation:

- `[VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraComponent.h:306-307,650,659-661]`
- `[VERIFIED: Engine/Source/Runtime/Engine/Classes/Kismet/KismetRenderingLibrary.h:38-48,141-144]`

## Live evidence

- `list_toolsets` exposed `UeremcpValidation.UeremcpVisualCaptureToolset`.
- `describe_toolset` exposed version `0.1.0` and
  `CaptureEffectFrames(requestJson: string)`.
- Request `visual-capture-known-sync-20260730` captured four 640×360 PNGs from
  `/Game/RE/VFX/Magecraft/Spells/NS_Spell_IceWall_Cast`; status
  `no_change_required`, `rendered_something=true`,
  `max_delta_lit_pixels=37`, all PNGs reread, and stage teardown complete.
- Independent Pillow decode of `age_03.png`: PNG, RGBA, 640×360, 157,982 bytes,
  non-flat channel extrema.
- The UEREMCP-created scratch probe had six emitters and compile-await evidence
  but no verified material bindings; capture correctly returned
  `failed_validation` with zero changed pixels. The duplicated scratch control
  also rendered no pixels, so the passing visual control remains the original
  read-only asset. Do not document scratch duplication as renderer-preserving
  until that Niagara behavior is separately explained.
- On a cold editor, the first IceWall request exported valid PNGs but observed
  zero changed pixels; the immediately following request observed 49 changed
  pixels. Inline simulation/render flushes cannot create the editor tick boundary
  the renderer needs. The tool reports the cold result as `failed_validation`;
  resolving this without a second MCP round trip requires ADR-0009 job scheduling
  across an editor tick. Do not mark the capability fully available before that
  asynchronous path is implemented and restart-repeatability passes.
