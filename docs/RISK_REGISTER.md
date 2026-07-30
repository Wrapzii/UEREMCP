# Risk Register

**Owner:** WS-01. Workstreams raise new risks via `docs/proposals/`.

Severity × likelihood, both High/Medium/Low. "Kills the project" means the core value
proposition does not survive.

| ID | Risk | Sev | Lik | Owner | Mitigation | Status |
|---|---|---|---|---|---|---|
| **R-01** | **Blueprint graphs cannot be faithfully reconstructed from JSON via public UE 5.8 API.** Exotic nodes (latent, delegates, macros, custom K2) resist generic construction. | High | Medium | WS-06 | **Research closed (RB-05):** simple/latent/switch DSL round-trip identical; MultiGate decompile fails on exec chain; Timeline special-case; GUID churn → `semantic_id`; hash structured JSON not DSL. POC A = envelope/bridge over Epic (not new primitives). Residual: `graph.schema.json` bridge impl + complex RE character ceiling. Wave 2 gated on R-04. | open |
| **R-02** | Experimental engine APIs churn. `ModelContextProtocol`, `ToolsetRegistry`, `Toolsets` are all `IsExperimentalVersion: true`; two are `NoRedist`. A hotfix or 5.9 breaks us. | High | Medium | WS-01 | ADR-0001. Adapter layer isolating registry types; domain services depend on neither plugin. Pin engine version in CI. Fast integration suite for engine bumps. | open |
| **R-03** | `FileSandbox` does not intercept package saves, so atomic multi-asset rollback does not work as ADR-0005 designs. | Medium | Low | WS-11 | **Mitigated for Content/ full-Discard:** shipping gate green on RE (`-KeepUeremcp -NoProbe`, `bf30d8f` / orch). q1/q3 POSITIVE. Residual: BP compile, deletions, `Saved|Config` — keep narrow `rollback.available`. | mitigated |
| **R-04** | `AICallable` UFUNCTION taking one JSON string generates a useless schema, so agents get no guidance and the envelope's discoverability collapses. | High | Medium | WS-03 | **Confirmed shape, host partial:** RB-03 q6 verbatim schema is bare `type:string` (+ description). Plugin compiles + automation Ping/Echo/Schema 3/3 (`595a73d`). **MCP `call_tool` still open:** live editor is `visualtest`, not RE — `UEREMCP` not discovered on `:8000`. Mitigate via `describe_action` / docs / optional `USTRUCT` hybrid (ADR-0003). | open |
| **R-05** | Niagara module stacks do not fit the shared graph representation, forcing a fork of ADR-0004. | Medium | Low | WS-07 | **Closed as fork risk (RB-07):** no ADR-0004 challenge — stacks map to `NiagaraModuleStack` + `extensions.niagara` (`ws-07-adr-0004-niagara-fit.md`). Residual fidelity: event-handler stacks missing from Epic topology; no ReorderModule AICallable. | mitigated |
| **R-06** | **Duplicating Epic.** 27 engine toolsets already cover many named domains; workstreams rebuild working tools. | Medium | Medium | WS-02 | **Partial close:** `source_complete` audits for Epic (875) + REAgentTools (62) with dispositions (`ab4c300`). Runtime MCP dumps still open. Keep rule 2; do not treat schemas as runtime-verified. | open |
| **R-07** | **Agents destroy user content** — deleting assets, undoing user transactions, overwriting concurrent work. The realistic threat is agent error at speed, not an attacker. | **High** | Medium | WS-12 | **Design frozen (ADR-0010):** destructive dry-run force, path gates, forbid agent `UndoTransaction`, audit under `Saved/UEREMCP/`. Residual: Wave 2 `UeremcpSecurity` implementation + tests. | open |
| **R-08** | AnimBP state-machine **authoring** unsupported on agent surface; Control Rig authoring exists via Epic tools but JSON round-trip fidelity unproven. (§9 cannot be full AnimBP SM replace.) | Medium | Medium | WS-10 | **Refined (RB-09):** CR create/hierarchy/RigVM verified under `/Game/__UeremcpTests/`; AnimBP SM readable (`list_graphs`) but `read_graph_dsl` empty / no write tools. Deliver montage/notify/socket + AnimBP inspect; compose `ControlRigTools`; do not rebuild AnimationAssistant (320). | open |
| **R-09** | Complete graph payloads exceed usable context for large Blueprints, making `response_detail: complete` unusable. | Medium | Medium | WS-06 | RB-05 q16 measures it. MCP resources are viable (`IModelContextProtocolResourceProvider`) per RB-04 / ADR-0009 — allowed complement for bulk `complete` bodies; not mandatory until measured. | open |
| **R-10** | Async compilation (Blueprint, Niagara, shaders) is not deterministically awaitable, producing flaky validation and unstable `content_hash`. | Medium | Medium | WS-11 | RB-14 q5, RB-05 q15, RB-07 q12. Never report compiled without confirming completion. | open |
| **R-11** | Modal dialogs or editor lockups hang tool calls indefinitely. REAgentTools built `UnrealWatchMCP` specifically for this, which is evidence it is real. | Medium | **High** | WS-12 | RB-04 confirmed game-thread marshalling + no engine tool timeout. ADR-0009: return job handle, never hang SSE. Modal protection stays **host-side** (UnrealWatchMCP / unreal-watch); not in-process transport. | open |
| **R-12** | Concurrent agents corrupt shared state — same asset, gameplay tag tables, project config. | Medium | **High** | WS-12 | **ADR-0010 mutator queue** (single writer per project) + ADR-0006 `reject`. Tag INI stores: avoid for RE POC D; otherwise single-writer / namespaced batches (`ws-09-tag-concurrency-adr0006.md`). Residual: implement queue. | open |
| **R-13** | Epic's MCP server has no authentication and may bind beyond loopback. | Medium | Low | WS-12 | ** empirically closed (RB-13):** no auth; Origin 403; runtime `127.0.0.1:8000`. ADR-0010: require loopback; do not fork Epic for tokens; warn/refuse mutators on non-loopback. Residual: operator discipline + best-effort bind detect. | open |
| **R-14** | Test cycles are too slow (editor launch + compile), so agents skip verification and the project's central premise quietly fails. | **High** | Medium | WS-11 | RB-14 q3, q10. Fast out-of-editor path for pure logic. If integration tests take 10 minutes, design acceptance around that reality. | open |
| **R-15** | The swarm produces disconnected research documents and no working code — master prompt §26's prohibited outcome. | **High** | Low | WS-01 | **Lowered again:** shipping Validation Rollback green on RE; four DLLs load; Python protocol 38/38. Residual Phase 1: MCP Ping on RE (live editor is `visualtest`), R-01/R-04/R-06 closeouts, C++ ContentHash golden mismatch (WS-05). | open |
| **R-16** | Protocol drift: workstreams quietly extend the envelope or fork the graph schema. | Medium | Medium | WS-01 | ADRs frozen. Schemas owned by WS-01. `tools/validate_schemas.py` in CI. `tools/check_ownership.py`. Challenges go through `docs/proposals/`. | open |
| **R-17** | The measured gain fails to beat the ~5:1 REAgentTools baseline, so the rebuild is not justified. | **High** | Low | WS-11 | Extend the existing benchmark harness so numbers are comparable (RB-14 q11). Report calls, tokens, **and completion rate**. Measure early, not at the end. | open |
| **R-18** | Engine `.cpp` source is unavailable, so behaviour can only be verified at runtime and much stays `[UNVERIFIED]`. | Medium | Medium | WS-01 | RB-01 q2. Broadcast the answer — it changes what verification tags everyone can honestly use. Lean harder on `[VERIFIED-RUNTIME]`. | open |

## The four that matter most

If effort must be concentrated, concentrate it here. All four are Wave 1:

1. **R-01** — RB-05. Decides whether the thesis holds.
2. **R-04** — RB-03. Decides whether the host model works.
3. **R-03** — RB-06. Decides whether batching works.
4. **R-06** — RB-02. Decides whether the effort is spent on the right things.

Every one of them is answerable in days by opening a header or running code in the
editor. None of them should still be open in week three.
