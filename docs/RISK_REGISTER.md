# Risk Register

**Owner:** WS-01. Workstreams raise new risks via `docs/proposals/`.

Severity × likelihood, both High/Medium/Low. "Kills the project" means the core value
proposition does not survive.

| ID | Risk | Sev | Lik | Owner | Mitigation | Status |
|---|---|---|---|---|---|---|
| **R-01** | **Blueprint graphs cannot be faithfully reconstructed from JSON via public UE 5.8 API.** Exotic nodes (latent, delegates, macros, custom K2) resist generic construction. | High | Medium | WS-06 | **Mitigation upgraded by RB-02:** Epic `BlueprintTools` already has `read_graph_dsl` / `write_graph_dsl` (+ 52 tools) `[VERIFIED: docs/audit/epic-toolsets.md q8]`. POC A is a schema/envelope/validation bridge over Epic, not pin authoring from scratch. Residual risk is DSL↔`graph.schema.json` fidelity + exotic nodes. | open |
| **R-02** | Experimental engine APIs churn. `ModelContextProtocol`, `ToolsetRegistry`, `Toolsets` are all `IsExperimentalVersion: true`; two are `NoRedist`. A hotfix or 5.9 breaks us. | High | Medium | WS-01 | ADR-0001. Adapter layer isolating registry types; domain services depend on neither plugin. Pin engine version in CI. Fast integration suite for engine bumps. | open |
| **R-03** | `FileSandbox` does not intercept package saves, so atomic multi-asset rollback does not work as ADR-0005 designs. | Medium | Low | WS-11 | **Partial close (engine path):** q1/q3 POSITIVE via probe plugin + `-DisablePlugins=UEREMCP` (`0e2e78d`). WS-14 C-3: not shipping-plugin evidence. Residual: BP/deletes/Config + green run with `UEREMCP` enabled. | open |
| **R-04** | `AICallable` UFUNCTION taking one JSON string generates a useless schema, so agents get no guidance and the envelope's discoverability collapses. | High | **High** | WS-03 | **Elevated:** WS-14 C-1 — `ws-03-plugin` has no commits; MCP-reachable tool unproven. RB-03 q6 schema capture + Ping/Echo runtime required. Uncommitted Core work exists in worktree — must land. | open |
| **R-05** | Niagara module stacks do not fit the shared graph representation, forcing a fork of ADR-0004. | Medium | **High** | WS-07 | `extensions.niagara` is the designed pressure valve. RB-07 q16. A well-evidenced ADR-0004 challenge is an acceptable outcome. | open |
| **R-06** | **Duplicating Epic.** 27 engine toolsets already cover many named domains; workstreams rebuild working tools. | Medium | Medium | WS-02 | **Partial close:** `docs/audit/epic-toolsets.md` source-verified 875 tools + do-not-rebuild list (2026-07-29). Runtime `list_toolsets`/`describe_toolset` dumps still missing (editor offline). Keep rule 2 enforcement; reopen likelihood if agents ignore the matrix. | open |
| **R-07** | **Agents destroy user content** — deleting assets, undoing user transactions, overwriting concurrent work. The realistic threat is agent error at speed, not an attacker. | **High** | Medium | WS-12 | `AGENTS.md` rule 8. `dry_run` default true on destructive ops. RB-13. RB-06 q12 on the `UndoTransaction` hazard. Audit log answering "what changed in the last hour." | open |
| **R-08** | Control Rig / AnimBP state machines are effectively read-only via public API, so §9 cannot be delivered as described. | Medium | **High** | WS-10 | RB-09. Document the ceiling honestly in `capability_notes` rather than promising authoring. Design the schema for what is achievable. | open |
| **R-09** | Complete graph payloads exceed usable context for large Blueprints, making `response_detail: complete` unusable. | Medium | Medium | WS-06 | RB-05 q16 measures it. MCP resources are viable (`IModelContextProtocolResourceProvider`) per RB-04 / ADR-0009 — allowed complement for bulk `complete` bodies; not mandatory until measured. | open |
| **R-10** | Async compilation (Blueprint, Niagara, shaders) is not deterministically awaitable, producing flaky validation and unstable `content_hash`. | Medium | Medium | WS-11 | RB-14 q5, RB-05 q15, RB-07 q12. Never report compiled without confirming completion. | open |
| **R-11** | Modal dialogs or editor lockups hang tool calls indefinitely. REAgentTools built `UnrealWatchMCP` specifically for this, which is evidence it is real. | Medium | **High** | WS-12 | RB-04 confirmed game-thread marshalling + no engine tool timeout. ADR-0009: return job handle, never hang SSE. Modal protection stays **host-side** (UnrealWatchMCP / unreal-watch); not in-process transport. | open |
| **R-12** | Concurrent agents corrupt shared state — same asset, gameplay tag tables, project config. | Medium | **High** | WS-12 | ADR-0006 optimistic concurrency with `reject` default. RB-04: multi-session OK at transport; **no write serialization**. Prefer one writer/project; optional mutator queue is Wave 2. | open |
| **R-13** | Epic's MCP server has no authentication and may bind beyond loopback. | Medium | Low | WS-12 | RB-04: no tokens; Origin localhost guard; client configs target `127.0.0.1`. Full bind/auth empirical check still RB-13. | open |
| **R-14** | Test cycles are too slow (editor launch + compile), so agents skip verification and the project's central premise quietly fails. | **High** | Medium | WS-11 | RB-14 q3, q10. Fast out-of-editor path for pure logic. If integration tests take 10 minutes, design acceptance around that reality. | open |
| **R-15** | The swarm produces disconnected research documents and no working code — master prompt §26's prohibited outcome. | **High** | **High** | WS-01 | **Elevated by WS-14:** protocol/transport/validation code exists, but host plugin (WS-03) uncommitted/unproven and rollback gate not on shipping graph. Mitigate by finishing WS-03 before Wave 2 code. | open |
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
