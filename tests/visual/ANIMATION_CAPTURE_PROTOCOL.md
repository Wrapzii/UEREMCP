# Animation capture protocol (WS-11)

**Status:** Static/contract ready; live editor proof deferred (sibling owns RE).
**Owner:** WS-11. **Related:** [`VISUAL_CAPTURE_PROTOCOL.md`](../../docs/VISUAL_CAPTURE_PROTOCOL.md).

## Action

`capture_animation_frames` on `UeremcpValidation.UeremcpVisualCaptureToolset`.

## Request shape

```json
{
  "protocol_version": "1.0",
  "request_id": "anim-cap-1",
  "action": "capture_animation_frames",
  "target": {"asset_path": "/Game/__UeremcpTests/Anim/AS_Probe"},
  "options": {"dry_run": false},
  "specification": {
    "skeletal_mesh_path": "/Game/__UeremcpTests/Meshes/SK_Probe",
    "frame_count": 4,
    "duration_seconds": 1.0,
    "camera": "three_quarter",
    "width": 960,
    "height": 540,
    "warm_up_ticks": 8
  }
}
```

APIs used (verified locally against UE 5.8 headers):

- `[VERIFIED: Engine/.../AnimSequenceBase.h:86]` `GetPlayLength`
- `[VERIFIED: Engine/.../SkeletalMeshComponent.h:1258,1267,1303,1804,2182]`
  `PlayAnimation` / `SetAnimation` / `SetPosition` / `TickAnimation` /
  `RefreshBoneTransforms`

## Structural gate (authoritative)

- `anim_sequence_path`, `skeletal_mesh_path`, `skeleton_path`
- `play_length_seconds` > 0
- `ref_bone_count` > 0
- `verification.play_length_positive` and PNG reread

## Visual (supplemental)

PNGs under `Saved/UEREMCP/AnimationCapture/<asset>/<request_id>/`. Pose correctness
is not judged from pixels alone.

## Live checklist

1. Tool registered; dry-run clean.
2. Missing mesh path → `rejected`.
3. Known sequence + mesh → frames at deterministic ages; bone count matches inspect.
4. No project content outside scratch; stage teardown complete.
