# WS-08: External flipbook sheet import — research and proposal

- **From:** WS-08
- **Date:** 2026-07-30
- **Status:** proposal — **Phase A scaffold** landed (`flipbook_import` parses, returns `partially_completed`); runtime import **not implemented**

## Problem

`generate: flipbook_atlas` packs **procedural per-frame noise cells** into a grid atlas
(`00006fe`). Production VFX often needs artist-authored PNG/TGA flipbook sheets imported
from ComfyUI, Photoshop, or external tools. Capability note: *no external sheet import*.

## Verified UE 5.8 API surface

| Operation | API | Tag |
|---|---|---|
| Import image buffer → Texture2D | `FImageUtils::ImportBufferAsTexture2D` | `[VERIFIED: ImageUtils.h:448-449]` |
| CPU pixel → Texture2D (existing path) | `FImageUtils::CreateTexture2D` | `[VERIFIED: ImageUtils.h:268]` |
| Save under `/Game` | `UEditorAssetSubsystem::SaveAsset` | `[VERIFIED: REAgentTools material_workflow_tools.py:67]` |

Epic `MaterialTools` has no flipbook assembler (`RB-08` §C10). Import is the viable path.

## Proposed design

### Schema extension (specification only)

Add optional branch to `create_procedural_texture` (or nested `textures` slot):

```json
{
  "generate": "flipbook_import",
  "source": {
    "file_path": "C:/absolute/or/project/relative/sheet.png",
    "flipbook": { "columns": 4, "rows": 4, "frame_count": 16 }
  }
}
```

Alternative: `source.base64` for agent-supplied buffers (size-gated; WS-12 audit).

### Service behavior

1. Read bytes from allowed roots (`/Game/__UeremcpTests/` import staging or envelope `source.file_path` under project).
2. `ImportBufferAsTexture2D` or decode PNG → validate dimensions match `columns * cell` expectation.
3. Optional grid re-pack if sheet is single strip (future).
4. Save to `target.asset_path`; re-read for `created_and_validated` when `validate:true`.
5. Honest `partially_completed` when import succeeds but grid metadata cannot be verified.

### Status contract

| Outcome | Status |
|---|---|
| Import + dimension re-read OK | `created_and_validated` |
| Import OK, validate:false | `partially_completed` |
| Import OK, grid metadata mismatch | `failed_validation` |
| Path outside allowed roots | `rejected` |
| Not implemented (now) | use `flipbook_atlas` procedural or `rejected` for `flipbook_import` |

## Phases

1. **Phase A (this proposal):** document APIs; offline guards; no runtime import.
2. **Phase B:** `flipbook_import` rejected with explicit message until Phase C.
3. **Phase C:** implement import path + editor automation under `/Game/__UeremcpTests/Textures/`.
4. **Phase D:** wire `textures.MainTexture.generate: flipbook_import` in `create_vfx_material`.

## Handoffs

| To | Need |
|---|---|
| WS-12 | Allowed import roots and size limits for external buffers |
| WS-11 | Editor test `CreateProceduralTexture.FlipbookImport` after Phase C |
| WS-07 | End-to-end: imported atlas + `flipbook_subuv` feature on MI |

## Current WS-08 status

- `flipbook_atlas`: CPU procedural grid assembly — **implemented**.
- `flipbook_import`: external sheet — **Phase A scaffold** (parses `source.file_path`; `ImportBufferAsTexture2D` not invoked).
