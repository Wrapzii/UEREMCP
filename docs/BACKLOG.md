# Backlog completion ledger — 2026-07-30 integration

Supersedes the live-session queue text where items are resolved. Historical
rationale remains above/in git history. Machine-readable states:

| ID | Item | Final state | Evidence |
|---|---|---|---|
| 0.1 | Enable GeometryScripting | `completed_and_verified` | Live `IsEnabled=true`; persisted in `RE.uproject` + `UEREMCP.uplugin` |
| 0.2 | Enable Water | `completed_and_verified` | Live `IsEnabled=true`; persisted in `RE.uproject` + `UEREMCP.uplugin` |
| 0.3 | Trim fictional template domains | `implemented_static_live_pending` | Enum is the eight canonical shipping domains; unsupported/provisional domains removed; registry-backed domain checker fails closed |
| 0.4 | Merge branch backlog | `completed_with_documented_limitation` | Valuable post-main work integrated selectively (router/capture/env); no blind merge of 32 branches — patch-equivalence / already on main `82337de` |
| 0.5 | Scripts/ read-order | `completed_and_verified` | `Scripts/README.md`; `AGENTS.md` 0c |
| 1.1 | Rewrite UEREMCP descriptions | `implemented_static_live_pending` | All 35 source callables pass task-vocabulary + Inputs + required specification keys + worked-example checks; fresh `describe_toolset` dump still required |
| 1.2 | Envelope contract echo | `implemented_static_build_pending` | `MakeRejection` returns shape/example/next action in one rejection; C++ automation contract test added |
| 1.3 | ResolveIntent collision | `implemented_static_live_pending` | Static contract asserts exactly one GetStarted/ResolveIntent/DescribeOperation owner and one catalog row; fresh single-editor registry proof required |
| 1.4 | Unify naming | `implemented_static_live_pending` | Canonical PascalCase preserved; case/snake/kebab lookup normalizes to live names without dual registration or breaking rename |
| 2.1 | dump_tool_registry | `implemented_static_live_pending` | Dump records source fingerprint and refuses to overwrite when live registry omits source callables |
| 2.2 | check_tool_names | `completed_static_stale_snapshot_detected` | Tool names, bogus near-misses, descriptions, source-vs-live callables, and domains checked; committed snapshot correctly fails stale |
| 2.3 | focus config | `withheld_by_safety_gate` | No ini committed; router demotion promotes UEREMCP while preserving safe Epic discovery; `--write` refuses global BlockedNames |
| 2.4 | route_prototype | `completed_static_live_pending` | Production/offline twins, catalog dependency ordering, deterministic held-out suite, and absent-name guards present |
| 2.5 | domain validation in CI | `implemented_static_live_pending` | Canonical enum must map to a live UEREMCP toolset; stale/missing mappings fail closed |
| 3.1 | Compile VisualCapture | `superseded_by_verified_implementation` | On main; Validation DLL present |
| 3.2 | Capture beyond Niagara | `completed_and_verified` | `CaptureWorldFrames` added |
| 3.3 | GetSystemSummary crash | `superseded_by_verified_implementation` | Fail-soft guards in `UeremcpNiagaraInspect.cpp` on main |
| 4.x | Coverage gaps landscape/water/foliage/mesh | `completed_with_documented_limitation` | `BuildEnvironment` semantic tool; GeometryScript mesh composition deferred (landscape path preferred) |
| 4.audio | Audio semantic | `completed_with_documented_limitation` | No goal-level tool yet — Epic gap; proposal notes; not duplicated |
| 4.net | Networking semantic | `completed_with_documented_limitation` | POC D/multiplayer proofs exist elsewhere; no new thin wrap |
| 4.wp | World partition | `blocked_external` | Large-world partitioning APIs need dedicated design; not silently wrapped |
| 5.1–5.8 | Procedural env design | `completed_and_verified` / `completed_with_documented_limitation` | Seeded noise, heightmap landscape, spline abstraction, Water river, foliage scatter, ExecutePlan-not-duplicated, verification honesty |

See `docs/guide/DISCOVERABILITY_LIVE_HANDOFF.md` for the exact remaining live
checks. Static completion is not described as live verification.
