# RB-03: `UToolsetDefinition` / `AICallable` mechanics from an out-of-tree plugin

- **Owner:** WS-03
- **Status:** findings_recorded (compile + automation verified; MCP client call blocked)
- **Blocks:** ADR-0002 confidence, ADR-0007, every tool in the project
- **Priority:** highest
- **Date:** 2026-07-29 / 2026-07-30

## Why this is first

ADR-0002 commits the entire project to hosting goal-level tools as static
`AICallable` `UFUNCTION`s on `UToolsetDefinition` subclasses inside an out-of-tree
editor plugin. That decision is grounded in headers
(`GROUNDED_FACTS.md §2.1`) but **had not been executed once**. This brief records
the first execution.

The deliverable is not only this document. It is a compiling plugin with Ping/Echo
plus these findings.

## Answers

### A. Does the basic path work at all?

1. **Yes.** Out-of-tree `UUeremcpReferenceToolset : UToolsetDefinition` with static
   `UFUNCTION(meta = (AICallable))` compiles and UHT generates wrappers.
   `[VERIFIED-RUNTIME: Build.bat REEditor -Module=UeremcpCore Result: Succeeded;
    Intermediate/.../UHT/UeremcpReferenceToolset.gen.cpp]`

2. **MCP client call: not yet verified.** Editor must load `UEREMCP` (needs
   `Binaries/Win64/UnrealEditor.modules` + often `-EnablePlugins=UEREMCP` after a
   prior load failure). Headless `UnrealEditor-Cmd` does not keep MCP `:8000`
   serving for the Cursor `user-unreal-mcp` client. Interactive editor launch was
   requested; at commit time `list_toolsets` against `127.0.0.1:8000/mcp` returned
   connection refused / transport error.
   `[VERIFIED-RUNTIME: user-unreal-mcp list_toolsets → WinError 10061;
    automation confirmed tools register via UToolsetRegistry::GetToolsetJsonSchema]`

3. **Plugin declare:**
   - `.uplugin`: `EditorOnly`, modules `Type: Editor`, `LoadingPhase: Default`,
     `TargetAllowList: ["Editor"]`; plugins `ToolsetRegistry` + `ModelContextProtocol`
     enabled.
   - `UeremcpCore.Build.cs`: private deps `UnrealEd`, `Json`, `JsonUtilities`,
     `Projects`, `ToolsetRegistry`.
   - After module-only builds, **write** `Binaries/Win64/UnrealEditor.modules`
     (`BuildId` matching engine, e.g. `55116800`) or the editor reports
     `module 'UeremcpCore' could not be found`.
     `[VERIFIED-RUNTIME: load failure without .modules; success after creating it]`

4. **Registration is NOT automatic.** Must call
   `UToolsetRegistry::RegisterToolsetClass`. Do **not** call it from bare
   `StartupModule` — `UToolsetRegistrySubsystem` / `GEditor` are unavailable
   (`AIToolsetRegistrySubsystem unavailable`). Defer to
   `FCoreDelegates::GetOnPostEngineInit()`.
   `[VERIFIED: $TR/.../ToolsetRegistrySubsystem.cpp:49 — UAgentSkillToolset]`
   `[VERIFIED-RUNTIME: StartupModule warning; PostEngineInit path in UeremcpCoreModule.cpp]`

### B. Envelope viability — q6 FIRST

5. **Yes.** `static FString Echo(const FString& RequestJson)` works as `AICallable`.
   `[VERIFIED-RUNTIME: UeremcpCore.ReferenceToolset.Echo automation Success]`

6. **VERBATIM generated JSON Schema for the single `requestJson` (FString) parameter**
   captured via `UToolsetRegistry::GetToolsetJsonSchema(UUeremcpReferenceToolset::StaticClass())`
   then extracting `tools[Echo].inputSchema.properties.requestJson`
   `[VERIFIED-RUNTIME: automation RegisterAndCaptureSchema; files under Plugins/UEREMCP/Saved/]`:

```json
{
	"type": "string",
	"description": "Request envelope JSON string (schemas/envelope/request.schema.json)."
}
```

   Full tool `inputSchema` for Echo (verbatim fragment of toolset schema):

```json
{
	"type": "object",
	"properties": {
		"requestJson": {
			"type": "string",
			"description": "Request envelope JSON string (schemas/envelope/request.schema.json)."
		}
	},
	"required": ["requestJson"]
}
```

   **Implication for ADR-0003:** the agent sees a bare string (plus our `@param`
   description). There is **no** structured envelope schema at the MCP tool
   boundary — discoverability of required fields (`protocol_version`, `action`, …)
   does **not** come from UHT. Envelope schema must be carried elsewhere
   (`describe_action` / docs / a `USTRUCT` parameter — see q7).

7. **Recommendation (no ADR challenge yet):** keep `FString` envelope for Ping/Echo
   proof; for production tools evaluate a hybrid (`USTRUCT` common fields + JSON
   `specification` string) or publish the frozen `schemas/envelope/request.schema.json`
   through `describe_action` / tool description text. Epic FakeToolset confirms
   FString → `{"type":"string"}` only
   `[VERIFIED: FunctionLibraryToolsetTest.cpp GetFakeToolsetExpectedSchema]`.

8. Deferred (not blocking Wave 1 Core). Epic FakeToolset covers TArray/TMap/TSet
   schema shapes in the same test file — cite when domain WSs need them.

### C. Async / private headers

9. Deferred beyond Wave 1 reference tools (sync `FString` return is enough for Ping/Echo).

10. **Private headers are NOT reachable** from an out-of-tree plugin without hacking
    include paths. `ToolsetRegistry.Build.cs` has empty `PublicIncludePaths`;
    `RunOnMainThread.h`, `JsonSchema.h`, `ValueOrErrorFuture.h` live under
    `.../Private/ToolsetRegistry/`.
    **Public equivalents:**
    - main-thread dispatch → `Async(EAsyncExecution::TaskGraphMainThread, ...)`
    - futures → `TValueOrError` / `TPromise` / `MakeFulfilledPromise`
    - schema → public `UToolsetRegistry::GetToolsetJsonSchema` / `FJsonSchemaGenerator`
    - registration → public `UToolsetRegistry::RegisterToolsetClass` only
    `[VERIFIED: ToolsetRegistry.Build.cs; header tree under Private/]`

11–12. Deferred (sync tools; no exception-path experiment this session).

### D. Discovery

13. Toolset schema name: `UeremcpCore.UeremcpReferenceToolset`; tools
    `...Ping` / `...Echo`. Descriptions come from class / UFUNCTION comment text
    (including verification tags in the class comment — trim those for production).
    `[VERIFIED-RUNTIME: rb03_echo_tool_schema.json]`

14. `SetNameFilters` not exercised this session.

### E. Iteration cost — q15

15. **Live Coding / hot reload blocks `Build.bat` while the editor holds modules.**
    Successful compiles required `-NoHotReloadFromIDE` and no competing editor lock
    (or editor closed). Changing `AICallable` signatures needs UHT + full module
    rebuild; do not rely on Live Coding for tool signature iteration.
    `[VERIFIED-RUNTIME: Build.bat ... -NoHotReloadFromIDE; prior session hang when
     editor held hot-reload lock]`
    ADR-0007 may still recommend Python for exploratory signatures; C++ remains
    correct for frozen tools.

## Compile / automation evidence

| Check | Result | Tag |
|---|---|---|
| `UeremcpCore` link | Succeeded — `UnrealEditor-UeremcpCore.dll` | `[VERIFIED-RUNTIME]` |
| `UeremcpTransport` link | Succeeded — `UnrealEditor-UeremcpTransport.dll` | `[VERIFIED-RUNTIME]` |
| `UeremcpProtocol` | Succeeded after mechanical UE 5.8 `FSharedString` key fixes — `UnrealEditor-UeremcpProtocol.dll` | `[VERIFIED-RUNTIME]`; see `docs/proposals/ws-03-protocol-ue58-json-keys.md` |
| `UeremcpValidation` | Succeeded — `UnrealEditor-UeremcpValidation.dll` | `[VERIFIED-RUNTIME]` |
| Automation Ping/Echo/Schema | 3/3 Success, exit 0 | `[VERIFIED-RUNTIME: automation_rb03.log]` |
| MCP Ping via `:8000` | Not verified (connection refused / proxy busy) | negative finding recorded |

## Temporary Core helpers

`Private/UeremcpMinimalEnvelope.h` — Ping/Echo only, until WS-05 ships a compiling
`UeremcpProtocol`. Not an ADR-0003 fork.

## Module registration (`.uplugin`)

| Module | Status |
|---|---|
| `UeremcpCore` | registered (owned) |
| `UeremcpTransport` | registered; sources checked out from `ws-04-transport` verbatim |
| `UeremcpProtocol` | **not** registered — non-compiling on UE 5.8 |
| `UeremcpValidation` | **pending** — sources only uncommitted on WS-11 worktree |

## API availability summary

| API / capability | Public | Editor-only | C++ | Python | Notes | Tag |
|---|---|---|---|---|---|---|
| `UToolsetDefinition` + `AICallable` | Y | Y | Y | Y | Out-of-tree works | `[VERIFIED-RUNTIME]` |
| `UToolsetRegistry::RegisterToolsetClass` | Y | Y | Y | — | Explicit; PostEngineInit | `[VERIFIED]` / `[VERIFIED-RUNTIME]` |
| `GetToolsetJsonSchema` | Y | Y | Y | — | FString → type+description only | `[VERIFIED-RUNTIME]` |
| Private `RunOnMainThread.h` etc. | N | Y | — | — | Use Engine public APIs | `[VERIFIED]` |
| Epic MCP HTTP `:8000` | Y | Y | — | — | Server plugin; client not verified this session | `[DOCS]` / negative runtime |

## Deliverables

- [x] `Plugins/UEREMCP` Core (+ Transport) compiles against UE 5.8
- [ ] `ping` and `echo` callable from an MCP client — blocked; schema/registration verified in-editor automation instead
- [x] Generated JSON Schema for envelope parameter pasted verbatim (q6)
- [x] q7 note recorded (no ADR challenge yet)
- [x] Findings on q10 and q15

