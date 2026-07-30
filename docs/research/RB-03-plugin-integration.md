# RB-03: `UToolsetDefinition` / `AICallable` mechanics from an out-of-tree plugin

- **Owner:** WS-03
- **Status:** not_started
- **Blocks:** ADR-0002 confidence, ADR-0007, every tool in the project
- **Priority:** highest — start immediately

## Why this is first

ADR-0002 commits the entire project to hosting goal-level tools as static
`AICallable` `UFUNCTION`s on `UToolsetDefinition` subclasses inside an out-of-tree
editor plugin. That decision is grounded in headers
(`GROUNDED_FACTS.md §2.1`) but **has not been executed once**. If it does not work as
read, every downstream decision changes. Find out in week one.

The deliverable is not a document. It is **a compiling plugin with one working tool**,
plus this brief recording what you learned.

## Questions

### A. Does the basic path work at all?

1. Can an out-of-tree editor plugin subclass `UToolsetDefinition` and have UHT accept
   static `UFUNCTION`s marked `meta = (AICallable)`?
2. Does the tool appear to an MCP client connected to `http://127.0.0.1:8000/mcp`?
3. What exactly must the plugin declare — module dependencies in `Build.cs`, plugin
   dependencies in `.uplugin`, loading phase, `TargetAllowList`?
4. Is registration automatic via reflection, or must something call
   `FToolsetRegistry::RegisterToolset`? The Python path uses an explicit
   `Registration(...).register()` `[VERIFIED: $TR/Content/Python/toolset_registry/registration.py]`
   — determine the C++ equivalent, and whether UHT-discovered `UToolsetDefinition`
   subclasses self-register.

### B. Envelope viability — the critical question

ADR-0003 requires one JSON string in, one JSON string out. Verify:

5. Does a signature like
   `static FString ExecuteFoo(const FString& RequestJson)` work as an `AICallable` tool?
6. **What JSON Schema does UHT/the registry generate for it?** If the agent only sees
   "a string parameter," it has no schema guidance and the envelope's discoverability
   collapses. This is the one finding most likely to force an ADR-0003 revision.
7. If (6) is a problem, what are the options? Candidates to evaluate:
   - a `USTRUCT` parameter, letting the registry generate a real schema
     (`UToolsetLibrary::ListStructProperties` suggests struct→schema exists
     `[VERIFIED: ToolsetLibrary.h]`)
   - a hybrid: typed `USTRUCT` for common envelope fields plus a JSON string for
     `specification`
   - carrying the real schema in `describe_action` instead
8. How are `TMap`, `TArray`, and nested `USTRUCT` parameters represented? Note that
   `UAgentSkillToolset` uses `TMap<FString, FString>` and
   `TMap<FString, FAgentSkillDetails>` return types successfully
   `[VERIFIED: AgentSkill.h]` — so structs in signatures demonstrably work.

### C. Async and threading

9. How does a tool return asynchronously? Which `UToolCallAsyncResult` derivative
   fits a JSON-returning tool — `ToolCallAsyncResultString`, or do we need our own?
10. Are `Private` headers (`RunOnMainThread.h`, `JsonSchema.h`, `ValueOrErrorFuture.h`)
    reachable from an out-of-tree plugin? If not, what is the public equivalent, and do
    we need to write our own main-thread dispatch? **Answer this before WS-05 designs
    the job model.**
11. What thread does a tool body run on? Editor asset APIs are largely main-thread-only.
12. How does `ToolCallExceptionHandler` behave — does a C++ exception or a `check()`
    failure inside a tool crash the editor, or is it converted to an error result?

### D. Discovery and filtering

13. With `bEnableToolSearch = true` (the default), how does our toolset appear through
    `list_toolsets` / `describe_toolset` / `call_tool`? Confirm the tool names, the
    description text source, and how much of `GetToolsetDescription()` reaches the agent.
14. Does `FToolset::SetNameFilters` reliably hide internal primitives while keeping them
    callable internally? This is our mechanism for ADR-0002 rule 5.

### E. Iteration cost

15. Does Live Coding work for adding or changing an `AICallable` `UFUNCTION`, or does
    every signature change need a full editor restart? This directly determines whether
    ADR-0007 should recommend a Python layer for exploratory work.

## Method

Build `Plugins/UEREMCP` with one module and one toolset exposing two tools: a trivial
`ping` returning a fixed envelope, and an `echo` taking the full request envelope and
returning it inside a response envelope. Connect a real MCP client. Observe.

Start from the scaffold at `Plugins/UEREMCP/` and from `UAgentSkillToolset` as the
reference implementation.

## API availability summary

Fill in:

| API / capability | Public | Editor-only | C++ | Python | Notes | Tag |
|---|---|---|---|---|---|---|

## Deliverables

- [ ] `Plugins/UEREMCP` compiles against UE 5.8
- [ ] `ping` and `echo` callable from an MCP client — `[VERIFIED-RUNTIME]`
- [ ] The generated JSON Schema for the envelope parameter, pasted verbatim
- [ ] A recommendation on question 7, if needed, as a proposal against ADR-0003
- [ ] Findings on 10 and 15, which WS-05 and WS-01 are waiting on
