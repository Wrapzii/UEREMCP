# Backlog completion ledger — 2026-07-30 integration

Supersedes the live-session queue text where items are resolved. Historical
rationale remains above/in git history. Machine-readable states:

| ID | Item | Final state | Evidence |
|---|---|---|---|
| 0.1 | Enable GeometryScripting | `completed_and_verified` | Live `IsEnabled=true`; persisted in `RE.uproject` + `UEREMCP.uplugin` |
| 0.2 | Enable Water | `completed_and_verified` | Live `IsEnabled=true`; persisted in `RE.uproject` + `UEREMCP.uplugin` |
| 0.3 | Trim fictional template domains | `completed_and_verified` | `template.schema.json` enum trimmed; `check_tool_names` domain fiction fail-closed |
| 0.4 | Merge branch backlog | `completed_with_documented_limitation` | Valuable post-main work integrated selectively (router/capture/env); no blind merge of 32 branches — patch-equivalence / already on main `82337de` |
| 0.5 | Scripts/ read-order | `completed_and_verified` | `Scripts/README.md`; `AGENTS.md` 0c |
| 1.1 | Rewrite UEREMCP descriptions | `superseded_by_verified_implementation` | Intent vocabulary landed on main (`eddde50`+); Environment + CaptureWorldFrames added with task vocabulary |
| 1.2 | Envelope contract echo | `completed_and_verified` | `MakeRejection` capability_notes echo (ported from Opus prototype) |
| 1.3 | ResolveIntent collision | `superseded_by_verified_implementation` | On main since `23b9eca`; live DLL exports GetStarted/ResolveIntent/DescribeOperation — multi-editor stale MCP sessions can hide them (restart required) |
| 1.4 | Unify naming | `completed_with_documented_limitation` | `docs/proposals/ws-01-tool-naming-convention.md` — aliases/normalization, no breaking rename |
| 2.1 | dump_tool_registry | `superseded_by_verified_implementation` | Present in tree |
| 2.2 | check_tool_names | `completed_and_verified` | Extended with domain fiction checks |
| 2.3 | focus config | `completed_with_documented_limitation` | Generated tooling present; **not enabled** until post-rebuild describe+echo verified live (BACKLOG gate) |
| 2.4 | route_prototype | `superseded_by_verified_implementation` | Present; production twin on main |
| 2.5 | domain validation in CI | `completed_and_verified` | `check_tool_names.check_domains` |
| 3.1 | Compile VisualCapture | `superseded_by_verified_implementation` | On main; Validation DLL present |
| 3.2 | Capture beyond Niagara | `completed_and_verified` | `CaptureWorldFrames` added |
| 3.3 | GetSystemSummary crash | `superseded_by_verified_implementation` | Fail-soft guards in `UeremcpNiagaraInspect.cpp` on main |
| 4.x | Coverage gaps landscape/water/foliage/mesh | `completed_with_documented_limitation` | `BuildEnvironment` semantic tool; GeometryScript mesh composition deferred (landscape path preferred) |
| 4.audio | Audio semantic | `completed_with_documented_limitation` | `UeremcpSystems.CreateAudioCue` / `InspectAudio` (SoundCue+attenuation). MetaSound graph authoring still blocked |
| 4.net | Networking semantic | `completed_with_documented_limitation` | `ValidateReplication` goal audit (+ optional configure). Multi-client proof remains WS-11/RB-14 |
| 4.wp | World partition | `completed_with_documented_limitation` | `InspectWorldPartition` + dry-run-default `RepairWorldPartition` via `CreateOrRepairWorldPartition`. HLOD builders blocked |
| 5.1–5.8 | Procedural env design | `completed_and_verified` / `completed_with_documented_limitation` | Seeded noise, heightmap landscape, spline abstraction, Water river, foliage scatter, ExecutePlan-not-duplicated, verification honesty |

See `docs/proposals/ws-01-backlog-completion-2026-07-30.md` for commands, SHAs, acceptance telemetry.
