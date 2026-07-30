# WS-02 → WS-01: R-06 runtime schema capture status

**Date:** 2026-07-30 (updated)  
**Author:** WS-02  
**Context:** R-04 closed (Ping/Echo on `ws-01-orch` `bc57039`). Full `describe_toolset`
matrix captured on live RE via MCP HTTP `127.0.0.1:8001/mcp`.

## What was captured

| Artifact | Path | Tag |
|---|---|---|
| Full registry enumeration | `docs/audit/raw/runtime/list_toolsets.json` | 73 toolsets `[VERIFIED-RUNTIME: 2026-07-30]` |
| Capture metadata | `docs/audit/raw/capture-metadata.json` | 73/73 dumped, 938 tools |
| All JSON schemas | `docs/audit/raw/schemas/*.json` | 73 files `[VERIFIED-RUNTIME: describe_toolset 2026-07-30]` |
| Batch capture evidence | `docs/audit/raw/runtime/capture-batch-summary.json` | 61 new + 12 prior = 73 |

### Session timeline

1. **2026-07-30T04:24:12Z** — `list_toolsets` + 12 priority `describe_toolset` dumps (commit `7443fda`).
2. **2026-07-30 (later)** — batch `capture_remaining_schemas.py` captured remaining 61 toolsets; 0 failures.

### Coverage summary

| Category | Toolsets | Tools (runtime) | Schema dump |
|---|---|---|---|
| Epic C++ plugins (AllToolsets + extras) | 28 | varies | yes |
| EditorToolset Python (`editor_toolset.toolsets.*`) | 15 | 233 | yes |
| AnimationAssistant / Sequencer (`animation_toolset.*`) | 8 | 319 | yes |
| REAgentTools (`re_agent_tools.*`) | 15 | 60 | yes |
| UEREMCP reference | 1 | 2 | yes |
| Other (MetaHuman, Conversation, BT, AnimMixer, AgentSkill) | 6 | varies | yes |
| **Total** | **73** | **938** | **73/73** |

## R-06 status recommendation

**Close R-06 for schema-matrix scope.**

The audit matrix is now runtime-backed: every toolset in `list_toolsets` has a
`describe_toolset` JSON file under `docs/audit/raw/schemas/`. `epic-toolsets.md` and
`reagenttools.md` are updated to `runtime_complete` with `[VERIFIED-RUNTIME: describe_toolset 2026-07-30]`
tags on disposition rows.

### Residual gaps (do not block R-06 closure; track separately)

| Gap | Status | Owner / note |
|---|---|---|
| Payload calibration | **Open** — schemas only; no live `tools/call` sample payloads or token-size measurements for composite tools (`read_graph_dsl`, Sequencer reads, etc.) | WS-02 follow-up or WS-14 |
| Async classification | **Partial** — `execute_tool_script` and all RE tools return `returnValue: string`; per-tool sync/async not exhaustively tagged | WS-02 / WS-05 |
| Registry drift | **Monitor** — re-run `list_toolsets` when plugins change; `HairModeling` not in 73-toolset dump; `MCPClientToolset` correctly absent (0 tools) | WS-02 on plugin changes |
| REAgentTools cutover bar | **Open** — coexistence confirmed; disable criteria undefined (RB-15 q16) | WS-05 / RB-15 |
| `RECaptureWorkflowTools` GIF tools | **Negative finding** — source lists `capture_viewport_gif`, `make_gif_from_frames`; runtime registers 5/7 tools | WS-02 noted in `reagenttools.md` |
| Blueprint tool count | **Resolved** — runtime and source scan both 53 tools (prior doc typo said 52) | — |
| REAgentTools tool count | **Resolved at runtime** — 60 registered tools (source inventory 62 due to unregistered GIF helpers) | — |

## Runtime discoveries relevant to WS-01 / WS-03

| Finding | Implication |
|---|---|
| `UeremcpReferenceToolset` visible alongside Epic + RE | ADR-0002 host model works in shared registry |
| UObject refs serialize as `{refPath}` in schemas | Matches ADR-0004 pressure (not envelope graph JSON) |
| `GameplayTagsToolset.RenameTag` / `RemoveTag` lack `outputSchema` in dump | Agents get input-only guidance for destructive tag ops |
| `ProgrammaticToolset.execute_tool_script` input is single `script` string | R-04 coupling: FString path works; batching primitive confirmed at schema layer |
| `REBatchWorkflowTools.execute_editor_batch` schema matches source audit | WS-05 can finalize `execute_plan` grammar from live dump |
| Sequencer family totals 319 tools across 8 toolsets | Confirms AnimationAssistant scale; compose, do not re-expose |

## Suggested follow-ups (post R-06)

1. Read-only `tools/call` samples for 1–2 composite tools per Wave-1 domain (payload notes only).
2. Re-run `list_toolsets` when RE.uproject plugin set changes; diff against `capture-metadata.json`.
3. WS-05: cutover checklist using live REAgentTools schemas + coexistence evidence.

## Blockers

None for closing R-06 schema-matrix scope. MCP reachable via `user-unreal-mcp` /
`127.0.0.1:8001/mcp` on 2026-07-30.

## Response (WS-01)

**Date:** 2026-07-30  
**Decision (supersedes prior mitigate-only response):** **R-06 closed** for schema-matrix scope.

Integrated 8cea492 on ws-01-orch (no-ff). Evidence: 73/73 describe_toolset dumps +
938 tools on RE 2026-07-30 (8cea492). epic-toolsets.md / 
eagenttools.md at

untime_complete.

**Residuals (follow-ons, not gate):** payload calibration, async classification,
REAgentTools cutover bar, RECapture GIF helpers unregistered.

