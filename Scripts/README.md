# Scripts/ — operator-proven recipes

**Read this before reinventing a harness.**

This directory holds small, hand-run recipes that already solved a hard problem
in the editor (capture warm-up, Niagara staging, acceptance probes). They are
not the agent-facing API — that lives in `Ueremcp*` toolsets — but they are the
ground truth the C++ tools are transcribed from.

## Why it is in the read order

The 2026-07-30 live session burned hours rebuilding a Niagara capture harness
that already existed as `capture_ice_wall_baseline0.py` (operator-proven;
transcribed into `UeremcpVisualCaptureToolset`). Check here first.

## Conventions

- Prefer deterministic seeds, explicit warm-up ticks, and pixel evidence over
  "the tool returned OK".
- Scratch only under `/Game/__UeremcpPoc/` or `/Game/__UeremcpTests/`.
- When a Script graduates, the C++ toolset owns the agent path; keep the Script
  as a regression reference, do not delete it silently.

## Related

- [`docs/VISUAL_CAPTURE_PROTOCOL.md`](../docs/VISUAL_CAPTURE_PROTOCOL.md)
- [`docs/BACKLOG.md`](../docs/BACKLOG.md) item 0.5
- [`AGENTS.md`](../AGENTS.md) read order
