# ADR-0002: Host model — in-process toolset plugin, not a new external MCP server

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-03 (plugin architecture), WS-04 (server architecture), WS-05 (protocol), all domain workstreams
- **Depends on:** ADR-0001

## Context

Master prompt §3.4 asks which language the external MCP server should be written in
— Python, TypeScript, C#, or Rust — and §3 sketches an architecture with a separate
MCP server process talking to an Unreal plugin over a custom transport.

Inspection of UE 5.8 shows that layer already exists in-engine, and that the
strongest option is not on the list.

Evidence:

- Epic's `ModelContextProtocol` plugin is a full MCP server implementation hosted
  inside the editor, with `ModelContextProtocolServer.h`, `...Session.h`,
  `...Capabilities.h`, `...ToolResults.h`, `IModelContextProtocolTool.h`
  `[VERIFIED: $MCP/Source/ModelContextProtocol/Public/]`. It serves HTTP on a
  configurable port and path, default `8000` and `/mcp`
  `[VERIFIED: ModelContextProtocolSettings.h]`, which is exactly what the RE
  project's client already connects to
  `[VERIFIED: $PROJ/.mcp.json]`.
- Tools are declared as static `UFUNCTION`s marked `meta=(AICallable)` on
  `UToolsetDefinition` subclasses, validated by UHT and the runtime registry
  `[VERIFIED: $TR/.../Public/ToolsetRegistry/ToolsetDefinition.h]`.
- The execution contract is already JSON-in/JSON-out and asynchronous:
  `TFuture<TValueOrError<FString, FString>> ExecuteTool(const FString& ToolName, const FString& JsonInput)`
  `[VERIFIED: $TR/.../Public/ToolsetRegistry/Toolset.h]`.
- Schema generation, async result types, exception handling, main-thread dispatch,
  property reflection, and name-pattern filtering are all provided
  `[VERIFIED: ToolsetRegistry.h, ToolCallAsyncResult.h, ToolCallExceptionHandler.h, RunOnMainThread.h, ToolsetLibrary.h, NamePatternFilter.h]`.
- A working reference implementation of an `AICallable` toolset ships in-engine:
  `UAgentSkillToolset` `[VERIFIED: $TR/.../Public/ToolsetRegistry/AgentSkill.h]`.

An external server process would sit *in front of* this and add a second hop, a
second serialization boundary, a second schema definition to keep in sync, and a
process-lifecycle problem — while having strictly less access to the editor than
in-process C++ does. The one thing it would add is the ability to run when the
editor is closed, which is not a requirement any POC needs.

## Decision

We will implement UEREMCP as an **in-process Unreal editor plugin** whose goal-level
operations are static `AICallable` `UFUNCTION`s on `UToolsetDefinition` subclasses,
served to agents through Epic's existing `ModelContextProtocol` server.

**There is no new external MCP server process.** Master prompt §3.4's language
question is resolved as "none of the above — C++ in-process," and §3's architecture
diagram collapses to:

```
AI Agent
    ↓  MCP over HTTP (Epic's ModelContextProtocol, port 8000 /mcp)
Epic MCP Server  ──►  ToolsetRegistry
    ↓                      ↓
    └──────────────►  UEREMCP toolsets  (UToolsetDefinition, AICallable statics)
                           ↓
                      UEREMCP domain services  (C++)
                           ↓
                Unreal editor subsystems / Epic toolsets / Python where justified
```

Specifics for implementers:

1. Plugin lives at `Plugins/UEREMCP/` in the target project, `EditorOnly: true`,
   depending on `ToolsetRegistry` and `ModelContextProtocol`.
2. Agent-facing surface: one `UToolsetDefinition` subclass per **domain**
   (`UUeremcpNiagaraToolset`, `UUeremcpBlueprintToolset`, ...), not per operation.
3. Every agent-facing tool takes a single JSON envelope string and returns a single
   JSON envelope string, per ADR-0003. This matches `FToolset::ExecuteTool`'s native
   contract, so no adaptation layer is needed.
4. Toolsets are thin. All real work lives in **domain services** — plain C++ classes
   with no `ToolsetRegistry` dependency — so the registry coupling sits in one
   replaceable layer (ADR-0001's churn mitigation).
5. Primitives that must exist internally are hidden from agents via
   `FToolset::SetNameFilters` block patterns, not by being absent.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| External Python MCP server + custom transport into Unreal | Adds a hop, a second schema source of truth, and process lifecycle management, while having less editor access than in-process C++. Duplicates Epic's server. |
| External TypeScript MCP server | Same as above. The mature-SDK argument is real but moot: we are not writing a server. |
| External Rust/C# server | Same as above, plus a third language in the build. |
| Python-only plugin, like REAgentTools today | Rejected as the *primary* layer by master prompt rule 20, and Python cannot reliably reach graph/compilation/transaction APIs. **Not rejected as a secondary layer** — see ADR-0007 (pending `RB-03`); `_reload.py` hot-reload is a genuine iteration advantage `[VERIFIED: $TR/Content/Python/toolset_registry/_reload.py]`. |
| Extend REAgentTools in place | Content-only Python plugin with no `Source/` `[VERIFIED: $RAT has no Source dir]`. Cannot host the C++ graph work. Its toolsets remain valuable and are migrated per `docs/audit/reagenttools.md`. |
| Fork Epic's `ModelContextProtocol` | See ADR-0001. Reconsider only if `RB-03` shows out-of-tree plugins cannot reach required API. |

## Consequences

**Enables:** direct access to editor subsystems, graph APIs, the transaction buffer,
the asset registry, and package saving from the same process and thread context that
executes the tool call. One schema source of truth. No IPC. No process supervision.
Existing MCP clients keep working against `127.0.0.1:8000/mcp` with no config change.

**Costs:**

- Iteration requires C++ compilation. Live Coding is enabled in the RE project
  `[VERIFIED: RE.uproject lists LiveCodingToolset]`, which helps, but this is slower
  than Python edit-and-reload. Mitigation: keep domain services testable outside the
  editor where possible; consider a Python layer for exploratory work (ADR-0007).
- The editor must be running. No headless/CI-without-editor path is assumed.
  Commandlet operation is an open question, not a promise.
- We inherit Epic's server's limits — auth, streaming, progress reporting, transport
  options are theirs, not ours (`RB-04`, `RB-13`).

**Locks in:** the `UToolsetDefinition` / `AICallable` declaration style across every
domain. Reversing this means rewriting every tool entry point — though the domain
services below it survive, which is precisely why rule 4 above exists.

## Open questions

- Are `Private` ToolsetRegistry headers (`RunOnMainThread.h`, `JsonSchema.h`,
  `ValueOrErrorFuture.h`) reachable from an out-of-tree plugin, or do we need
  equivalents? This is the single biggest implementation unknown. (`RB-03`)
- Does Epic's server support stdio in addition to HTTP? (`RB-04`)
- Does it authenticate at all? Assume not until proven. (`RB-13`)
- How do `AICallable` UFUNCTION signatures constrain envelope shape — can a single
  `FString` parameter carry our envelope cleanly, and how is the generated schema
  presented to the agent? (`RB-03`, first thing WS-03 should test.)

## Verification

An implementer complies if:

- their toolset class derives from `UToolsetDefinition` and its tools are static
  `UFUNCTION`s with `meta=(AICallable)`
- no new process is spawned to serve MCP
- domain-service headers include nothing from `ToolsetRegistry/` or
  `ModelContextProtocol/`
- the tool is reachable from an MCP client at the project's configured endpoint
