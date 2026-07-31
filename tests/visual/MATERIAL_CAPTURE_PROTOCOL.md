# Material capture protocol (WS-11)

**Status:** Static/contract ready; live editor proof deferred (sibling owns RE).
**Owner:** WS-11. **Related:** [`VISUAL_CAPTURE_PROTOCOL.md`](../../docs/VISUAL_CAPTURE_PROTOCOL.md).

## Action

`capture_material_frames` on `UeremcpValidation.UeremcpVisualCaptureToolset`.

## Request shape

```json
{
  "protocol_version": "1.0",
  "request_id": "mat-cap-1",
  "action": "capture_material_frames",
  "target": {"asset_path": "/Game/__UeremcpTests/Materials/M_Probe"},
  "options": {"dry_run": false},
  "specification": {
    "frame_count": 2,
    "camera": "three_quarter",
    "width": 960,
    "height": 540,
    "warm_up_ticks": 8
  }
}
```

## Structural gate (authoritative)

Response `structural` must include:

- `material_path`, `material_class`
- `is_instance` (+ `parent_path` when instance)

`verification.rendered_something` is a pixel-delta vs empty-stage baseline only.

## Visual (supplemental)

PNGs under `Saved/UEREMCP/MaterialCapture/<asset>/<request_id>/`. Never sole success
(POC_ACCEPTANCE B10).

## Live checklist

1. `list_toolsets` shows CaptureMaterialFrames.
2. Dry-run → `no_change_required`, no files.
3. Known engine/basic material → PNG reread + non-zero delta.
4. Missing material → `rejected`, no crash.
