# Grounded Facts — UE 5.8 Agent/MCP Surface

**Status:** authoritative. **Owner:** Lead Architect (WS-01) only.
**Last verified:** 2026-07-29 against a local UE 5.8 install.

> Every fact below was read out of engine source or project files on this machine.
> Do **not** edit this file to add unverified claims. If you need to record something
> you have not personally read in source, put it in your own research brief with a
> `[UNVERIFIED]` tag and cite where you looked.

## 0. Why this file exists

Fourteen concurrent agents re-deriving the same engine facts produces fourteen
subtly different — and often hallucinated — mental models. The single most common
failure mode in this project will be an agent asserting that an Unreal API exists
when it does not. This file is the shared floor. Read it before writing any design
doc, and cite it (`GROUNDED_FACTS.md §N`) instead of re-researching.

Paths below are abbreviated:

- `$ENGINE` = `$UE_ROOT/Engine`
- `$MCP` = `$ENGINE/Plugins/Experimental/ModelContextProtocol`
- `$TR` = `$ENGINE/Plugins/Experimental/ToolsetRegistry`
- `$TS` = `$ENGINE/Plugins/Experimental/Toolsets`
- `$PROJ` = `$UEREMCP_LEGACY_PROJECT`
- `$RAT` = `REAgentTools`

---

## 1. The engine ships its own MCP server. We are not building the first one.

`$MCP/ModelContextProtocol.uplugin`:

```json
"FriendlyName": "Unreal MCP",
"Description": "Anthropic MCP (Model Context Protocol) server implementation for Unreal Engine.",
"CreatedBy": "Epic Games, Inc.",
"IsExperimentalVersion": true,
"EnabledByDefault": false,
"NoRedist": true
```

Modules: `ModelContextProtocol` (Runtime), `ModelContextProtocolEngine` (Runtime),
`ModelContextProtocolEditor` (Editor), plus three test modules. It depends on
`EngineAssetDefinitions` and `ToolsetRegistry`.

Public headers in `$MCP/Source/ModelContextProtocol/Public/`:

| Header | Significance |
|---|---|
| `IModelContextProtocolTool.h` | Tool interface |
| `IModelContextProtocolResourceProvider.h` | MCP resources |
| `ModelContextProtocolServer.h` | Server |
| `ModelContextProtocolSession.h` | Session/lifecycle |
| `ModelContextProtocolCapabilities.h` | Capability negotiation |
| `ModelContextProtocolToolResults.h` | Result types |
| `ModelContextProtocolMetaData.h` | Tool metadata |
| `ModelContextProtocolAnalytics.h` | Instrumentation |

And in `ModelContextProtocolEngine/Public/`: `ModelContextProtocolSettings.h`,
`ModelContextProtocolToolLibrary.h`, `ModelContextProtocolToolAsyncAction.h`,
`ModelContextProtocolClientConfig.h`.

**Implication:** MCP transport, session management, capability negotiation, and
tool-result marshalling are solved in-engine. A workstream proposing to write a new
external MCP server must first explain what this does not provide. See
`adr/ADR-0002`.

### 1.1 Server configuration

`$MCP/Source/ModelContextProtocolEngine/Public/ModelContextProtocolSettings.h`,
`UModelContextProtocolSettings` (config = `EditorPerProjectUserSettings`):

| Property | Default | Meaning |
|---|---|---|
| `ServerUrlPath` | `/mcp` | URL base path |
| `ServerPortNumber` | `8000` | HTTP port |
| `bAutoStartServer` | `false` | Register HTTP route + start listeners at module startup |
| `bEnableToolSearch` | `true` | See §1.2 |

Free functions: `UE::ModelContextProtocol::GetServerUrlPath()`,
`GetServerPortNumber()`, `ShouldAutoStartServer()`.

This matches `$PROJ/.mcp.json`, which points a client at
`http://127.0.0.1:8000/mcp` over `"type": "http"`.

### 1.1a Transport shape (from RB-04 — do not re-derive)

Epic MCP is **HTTP only** — no stdio in the plugin
`[VERIFIED: ModelContextProtocolServer.h:23-24; grep $MCP — zero stdio matches]`.
`tools/call` uses Streamable HTTP / SSE (`text/event-stream`); control methods
return plain JSON `[VERIFIED: ModelContextProtocolServer.cpp:840-846, 892-895]`.
GET on the MCP path returns 405 — no persistent push channel
`[VERIFIED: ModelContextProtocolServer.cpp:1066-1075]`.

Protocol versions include `2025-11-25` (also `2025-06-18`, `2024-11-05`)
`[VERIFIED: ModelContextProtocol.h:19-28]`. Initialize advertises tools + resources
`[VERIFIED: ModelContextProtocolServer.cpp:677-680]`.
`IModelContextProtocolResourceProvider` is the public registration surface for
`resources/list` / `resources/read`
`[VERIFIED: IModelContextProtocolResourceProvider.h:30-33]`.

Progress: heartbeat `notifications/progress` when the client supplies
`progressToken` — monotonic integers, not percent-complete
`[VERIFIED: ModelContextProtocolServer.cpp:1036-1057, 1052-1053]`.
Cancellation: MCP `notifications/cancelled` calls `CancelAsync`, but the
ToolsetRegistry adapter does **not** override it
`[VERIFIED: ModelContextProtocolServer.cpp:717-719;
ModelContextProtocolToolsetRegistryAdapter.h:13-26]`.
No engine job IDs. Auth: Origin localhost guard; no bearer/token auth in the
reviewed surface. Client configs bind `127.0.0.1`
`[VERIFIED: ModelContextProtocolClientConfig.cpp:158]`.

**Implication:** long work uses the UEREMCP poll-after-timeout job model
(`ADR-0009`), not a second transport. Handoff JSON:
`Plugins/UEREMCP/Source/UeremcpTransport/constraints/transport_job_handoff.json`.

### 1.2 Tool search already exists — §2.9 of the master prompt is partly solved

Verbatim from `ModelContextProtocolSettings.h`:

> If true, `tools/list` returns only `list_toolsets`, `describe_toolset`, and
> `call_tool`. The LLM discovers toolset tools on demand and dispatches them through
> `call_tool` without ever registering them as native MCP tools. If false, every
> toolset tool is registered natively at startup.

Default is `true`. So the "compact discovery system" requirement (`list_domains`,
`describe_domain`, `get_schema`) has an existing engine analogue. Design **with**
it, not around it. Our `describe_domain` / capability-manifest work should layer
semantic grouping on top of `list_toolsets`/`describe_toolset`, not replace them.

---

## 2. ToolsetRegistry is the tool-authoring layer

`$TR/ToolsetRegistry.uplugin`: Editor-only, experimental, `CanContainContent: true`,
one Editor module. Depends on `PythonScriptPlugin`, `EditorScriptingUtilities`,
and **`FileSandbox`** (see §2.5 — this matters a lot).

### 2.1 Declaring tools in C++

`$TR/Source/ToolsetRegistry/Public/ToolsetRegistry/ToolsetDefinition.h` —
`UToolsetDefinition : public UObject`, `Abstract`, `BlueprintType`, `MinimalAPI`.
Header comment, verbatim:

> This is the common base class for Toolsets defined as UObjects.
> UFunctions that define tools in this class should be static functions and be
> marked with `meta = (AICallable)`. This is used both by UHT and the runtime
> UToolRegistry. UFunctions which should be ignored by tools should be marked with
> `meta = (AIIgnore)` in order to silence errors about invalid UFunctions.

API: `virtual FString GetToolsetVersion() const` (returns `"1.0"`; note the comment
that it is **called on the class default object**), and
`static TValueOrError<bool, FString> IsFunctionAICallable(const TObjectPtr<const UFunction>&)`.

**This is how we register goal-level operations.** A static `UFUNCTION` with
`meta=(AICallable)` on a `UToolsetDefinition` subclass becomes an MCP tool with a
UHT/reflection-generated JSON Schema. Working example in-engine:
`UAgentSkillToolset` (§4).

### 2.2 The toolset handler contract

`$TR/.../Public/ToolsetRegistry/Toolset.h` — `UE::ToolsetRegistry::FToolset`:

```cpp
TFuture<TValueOrError<FString, FString>> ExecuteTool(const FString& ToolName,
                                                     const FString& JsonInput);
FString GetJsonSchema() const;
virtual FString GetToolsetName() const = 0;
virtual FString GetToolsetVersion() const = 0;
virtual FString GetToolsetDescription() const = 0;
virtual UClass* GetToolsetClass() const;
bool IsEnabled() const;  void SetEnabled(bool);
void SetNameFilters(const TArray<FString>& BlockPatterns,
                    const TArray<FString>& AllowPatterns);
```

Three facts to design around:

1. **The native contract is already "JSON string in, JSON string out, async."**
   Our request/response envelope (`schemas/envelope/`) fits this natively — it is
   not something we have to bolt on.
2. Tool execution is **future-based**, so long-running jobs (§18 of the master
   prompt) have engine-level support. Whether that is sufficient for
   multi-minute jobs with progress reporting is an **open question** — see
   `research/RB-04`.
3. `SetNameFilters` supports substring and `/regex/` patterns, block-over-allow
   precedence, and patterns matching the toolset name toggle the whole toolset.
   This is our lever for hiding primitives from agents (master prompt §2.1) without
   deleting them.

### 2.3 The registry

`$TR/.../Public/ToolsetRegistry/ToolsetRegistry.h` —
`UE::ToolsetRegistry::FToolsetRegistry`:

- `bool RegisterToolset(TSharedPtr<FToolset>)` / `UnregisterToolset(...)`
- `TSharedPtr<FToolset> Find(...)`
- two `ExecuteTool(...)` overloads returning `TFuture<TValueOrError<FString, FString>>`
- `void ForEachToolset(...)`
- `FString GetToolsetJsonSchemas() const`
- name filtering: `AddBlockedName` / `RemoveBlockedName` / `GetBlockedNames`,
  and the `Allowed` equivalents
- converter registry: `RegisterConverter(TSharedPtr<FToolsetJsonConverter>)`,
  `UnregisterConverter`, `GetConverterForProperty(...)`
- `struct FToolDescriptor` with `static TValueOrError<FToolDescriptor, FString> FromString(...)`

`ToolsetRegistrySubsystem.h` — `UToolsetRegistrySubsystem : public UEditorSubsystem`
with `static TValueOrError<TObjectPtr<UToolsetRegistrySubsystem>, FString> Get(...)`,
plus `UToolsetRegistrySettings : public UDeveloperSettings` (category `Plugins`) which
carries class-pattern and `"ClassName.PropertyName"` property-pattern filters.

**`FToolsetJsonConverter` is the extension point for custom type marshalling** —
relevant to graph schemas, since we will want stable graph/pin/node types to
serialize predictably. See `research/RB-05`.

### 2.4 Async results

`$TR/.../Public/ToolsetRegistry/ToolCallAsyncResult.h` — `UToolCallAsyncResult : UObject`,
"analogous to a promise". Derivatives ship for the common cases:
`ToolCallAsyncResultVoid.h`, `ToolCallAsyncResultString.h`,
`ToolCallAsyncResultImage.h`, `ToolCallAsyncResultFutureHandler.h`.

Key members: `FString GetValueAsJsonString() const`,
`virtual TSharedPtr<FJsonValue> GetValueAsJson() const`,
`bool SetError(const FString&)`, `bool BroadcastOnCompletedIfComplete()`,
`bool MaybeBroadcastCompletion(TFunction<void()>&&)`,
`static TSharedRef<FJsonObject> GetValueJsonSchema(TSubclassOf<UToolCallAsyncResult>)`,
`static UClass* MatchesProperty(TNotNull<const FProperty*>)`.

Header note, verbatim: asynchronous tools should *not* use the base class directly;
they should define or use an existing derivative.

Also present and relevant: `ToolCallExceptionHandler.h`, and (private)
`RunOnMainThread.h`, `ValueOrErrorFuture.h`, `JsonSchema.h`, `JsonConversion.h`,
`PropertyAccessors.h`, `NamePatternFilter.h`, `ObjectFunctionToolCall.h`,
`FunctionLibraryToolset.h`, `PythonTestRunner.h`.

**Main-thread dispatch and tool-call exception handling are engine-provided.**
Do not write our own. `RunOnMainThread.h` is `Private`, so confirm what is
reachable from an out-of-tree plugin — `research/RB-03`.

### 2.5 Transactions and rollback have an engine foundation — do not build this from scratch

`$TR/.../Public/ToolsetRegistry/SandboxLibrary.h` —
`UE::ToolsetRegistry::FGlobalSandbox`, "static access to the globally active
FileSandbox instance":

```cpp
static bool IsActive();
static FString GetActiveName();
static bool Enter(const FString& Name, const FString& Description);
static bool Leave();
static TArray<UE::FileSandboxCore::FSandboxedFileChangeInfo> GetChanges();
static bool Persist(const TArray<FString>& Files);
static bool Discard();
static bool DiscardFiles(const TArray<FString>& Files);
```

This is an `Enter` → mutate → inspect `GetChanges()` → `Persist` or `Discard`
lifecycle over the file system, from the `FileSandbox` plugin.

Separately, `$TR/.../Public/ToolsetRegistry/ToolsetLibrary.h` —
`UToolsetLibrary : UBlueprintFunctionLibrary` exposes
`static bool UndoTransaction(bool bCanRedo = false)` and
`static int32 GetActiveUndoCount()`, i.e. editor-transaction control.

**Implication for master prompt §2.8:** we have two complementary mechanisms —
`FileSandbox` for package/file-level atomicity and rollback, and the editor
transaction buffer for in-memory undo. The design question is how they compose for
a multi-asset batch that includes compilation. That is a **primary open question**,
assigned to `research/RB-06`. Do not assume either one alone is sufficient.

### 2.6 Generic property/JSON plumbing

Also on `UToolsetLibrary`:

```cpp
static FString ListStructProperties(...);   // struct -> JSON Schema
static FString GetObjectProperties(const UObject*, const TArray<FName>&);
static bool SetObjectProperties(UObject*, const FString& PropertiesJson, ...);
static TArray<FSoftClassPath> GetDerivedClasses(UClass*);
static TArray<UScriptStruct*> GetDerivedStructs(UScriptStruct*);
```

`SetObjectProperties` is documented as strict: unknown keys cause failure and raise
(example in header: `{"name": "ok", "doesNotExist": 0}` returns false). There is
also an `EBypassContainerCheck` enum. Reflection-driven property get/set with schema
generation is therefore already available.

### 2.7 Python registration path

`$TR/Content/Python/toolset_registry/` — `registration.py` exposes
`class Registration` with `__init__(self, toolset_classes: Sequence[unreal.ToolsetDefinition])`,
`register() -> bool`, `unregister() -> None`, and a `_registry()` accessor typed
`ToolsetRegistryProtocol`. Sibling modules: `agent_skill.py`, `tool_call_impl.py`,
`helpers.py`, `_registry_interface.py`, `_reload.py`, `_unreal.py`, `_asyncio/`,
`tests/`.

Note `_reload.py` — Python toolsets can be hot-reloaded. That is a real iteration-speed
argument for Python that C++ does not match without Live Coding.

---

## 3. Epic ships 27 domain toolsets

Directories under `$TS`:

```
AIModuleToolset          AllToolsets               AnimationAssistantToolset
AutomationTestToolset    ChaosClothAssetToolset    ConfigSettingsToolset
ConversationToolset      DataRegistryToolset       DataflowAgent
EditorToolset            GASToolsets               GameFeaturesToolset
GameplayTagsToolset      LiveCodingToolset         MCPClientToolset
MVVMToolset              MetaHumanGenerator        NiagaraToolsets
PCGToolset               PhysicsToolsets           PluginToolset
SemanticSearchToolset    SequencerAnimMixerToolset SlateInspectorToolset
StateTreeToolset         UMGToolSet                WorldConditionsToolset
```

Most are C++ (`Source/` present); several also ship `Content/` (Python or assets).

**This is the single most important scoping fact in the project.** The master prompt
asks for domain coverage across Niagara, GAS, animation, PCG, StateTree, UMG,
physics, gameplay tags, and automation testing — and Epic already has a toolset for
each of those names. Our differentiator is therefore **not** "reach these domains
first." It is:

- goal-level semantic composition across domains in one request,
- complete graph round-trip as structured JSON,
- verification-before-success,
- batch/transaction/rollback across many assets,
- a reusable template & pattern library.

Every workstream must audit its Epic counterpart before proposing new primitives.
`WS-02` owns the matrix; domain workstreams own their own rows.

> `AllToolsets` is an aggregator. `MCPClientToolset` lets the editor act as an MCP
> *client* — worth understanding, since it may let Unreal call out to Blender or
> other MCP servers (master prompt §6). Assigned to `research/RB-11`.

---

## 4. `AgentSkill` is Epic's template/pattern-library primitive

`$TR/.../Public/ToolsetRegistry/AgentSkill.h`:

- `struct FAgentSkillDetails`
- `class UAgentSkill : public UObject` with `FAgentSkillDetails GetDetails() const`
  and a `BlueprintNativeEvent` `GeneratePrompt(const FString& InitialInstructions)`
  (`GeneratePrompt_Implementation` is virtual). Friends: `FAgentSkillSpec`,
  `UAgentSkillToolset`.
- `class UAgentSkillToolset : public UToolsetDefinition` with four `AICallable`
  statics:

```cpp
static TMap<FString, FString>            ListSkills();
static TMap<FString, FAgentSkillDetails> GetSkills(const TArray<FString>& SkillPaths);
static FString                           CreateSkill(...);
static bool                              UpdateSkill(...);
```

Skills are UObject assets, listable, describable, and **creatable and updatable by
the agent at runtime**.

**Implication for master prompt §10 and §5.3:** the "promote a successful effect
into a reusable template" and "agent writes new templates" requirements have an
engine-native substrate. `UAgentSkill` is prompt-shaped (`GeneratePrompt`), whereas
our templates need a structured `construction_plan` + `validation_rules`. Whether
we subclass `UAgentSkill`, sit beside it, or ignore it is a genuine design decision
— assigned to `research/RB-10`. This is also the best reference implementation of an
`AICallable` toolset to copy from.

---

## 5. The RE project

`$PROJ/RE.uproject`: `"EngineAssociation": "5.8"`. One Runtime module `RE`.
Source at `$PROJ/Source/RE/{Public,Private}`. Project plugins: `REAgentTools`,
`RECore`, `VoxelFree`.

Enabled plugins relevant to us: `ModelContextProtocol`, `MCPClientToolset`,
`AllToolsets`, `EditorToolset`, `AIModuleToolset`, `AnimationAssistantToolset`,
`AutomationTestToolset`, `ChaosClothAssetToolset`, `ConfigSettingsToolset`,
`ConversationToolset`, `HairModelingToolset`, `LiveCodingToolset`, `MVVMToolset`,
`SequencerAnimMixerToolset`, `GameplayStateTree`, `ModelingToolsEditorMode`,
`MetaHumanGenerator`, `LandscapePatch`, `Avalanche`, `MeshTerrainMode`, `VoxelFree`,
`PCGWaterInterop`, `MeshPartitionWater`, `WaterAdvanced`.

`$PROJ/.mcp.json` registers two servers: `unreal-mcp` (http, `127.0.0.1:8000/mcp`)
and `blender` (`uvx blender-mcp`).

> Note: `GASToolsets`, `NiagaraToolsets`, `GameplayTagsToolset`, `PCGToolset`,
> `StateTreeToolset`, `UMGToolSet`, `PhysicsToolsets`, `SemanticSearchToolset`,
> and `DataRegistryToolset` exist in the engine but are **not** listed as enabled in
> `RE.uproject`. Confirm actual load state at runtime before claiming a tool is or
> is not reachable — `.uproject` is not the whole story (`AllToolsets` may pull some
> in). This is a concrete first-day task for `WS-02`.

## 6. REAgentTools as it stands

`$RAT` — `REAgentTools.uplugin` v1.2.2, `EditorOnly: true`,
`CanContainContent: true`, `EnabledByDefault: true`, depends on `ToolsetRegistry`.
**There is no `Source/` directory: it is Python + content only.** ~6,385 lines of
Python under `Content/Python/`.

Layout:

- `Content/Python/init_unreal.py` — calls `toolsets._registration.register()`, warns
  "ToolsetRegistry not available — enable ModelContextProtocol" on failure.
- `re_agent_tools/common/` — `agent_policy`, `anim_helpers`, `blueprint_helpers`,
  `component_helpers`, `limits`, `logging`, `properties`, `resolution`, `results`,
  `serialization`, `spawn_helpers`, `transactions`, `validation`
- `re_agent_tools/toolsets/` — 15 toolsets: `actor_workflow`, `anim_workflow`,
  `asset_workflow`, `batch_workflow`, `blueprint_workflow`, `capture_workflow`,
  `character_workflow`, `context`, `dress_workflow`, `level_workflow`,
  `lighting_workflow`, `material_workflow`, `niagara_workflow`, `project_workflow`,
  `validation_workflow`
- `re_agent_tools/rc_bridge.py` + `Content/Python/_rc_reagent_exec.py` — Remote
  Control fallback for when client MCP discovery fails
- `Optional/UnrealMcpProxy/` and `Optional/UnrealWatchMCP/` — external Python
  helpers; the latter detects Slate dialogs/lockups
- `Docs/` — 11 markdown files including `CAPABILITY_MATRIX.md`, `TOOL_CATALOG.md`,
  `RESEARCH.md`, `NIAGARA_BATCHING.md`, `BENCHMARK_REPORT.md`, `EXPAND_PLAN.md`

`Config/DefaultREAgentTools.ini` exists (limits/policy config).

### 6.1 Gaps REAgentTools itself already documents

From its own `Docs/CAPABILITY_MATRIX.md` — these are the authors' own assessments,
so treat as claims to re-verify, not as verified engine facts:

| Area | Documented state |
|---|---|
| Blueprint graph/node authoring | ❌ absent — defers to Epic `BlueprintTools` / `BlueprintNodeTools` |
| `create_or_update_blueprint` | 🔶 class defaults only, no graph authoring |
| Niagara system/emitter/renderer authoring | ❌ absent — defers to Epic `NiagaraToolsets` batched through **one** `ProgrammaticToolset.execute_tool_script` |
| Master material graph editing | ❌ absent — defers to Epic `MaterialTools` |
| GAS ability graph authoring | 🔶 explicitly out of scope; `DT_Abilities` + `CastAbility` exist |
| Enemy AI / spawners, dungeon proc-gen | 🚫 "project architecture missing" |
| `run_map_check`, `get_recent_errors_compact` | 🔶 API may be unavailable; honest fallback |

**The three biggest holes are exactly our three highest-value targets: Blueprint
graph round-trip, Niagara construction, and material graph authoring.** That is
the thesis of this project, and it is consistent with the existing work rather than
a repudiation of it.

### 6.2 Prior art worth reusing rather than rediscovering

- `ProgrammaticToolset.execute_tool_script` — an **existing engine batching
  primitive**. REAgentTools' `NIAGARA_BATCHING.md` documents a working pattern:
  many Epic Niagara calls in one script, compile once at the end. Read that
  document before designing `execute_plan` (`WS-05`, `WS-07`).
- `execute_editor_batch` — REAgentTools' own batch tool with 8 allowlisted actions,
  `$ref` result chaining, and `dry_run`. This is a direct precursor to our batch
  schema, including the `$ref` idea. Audit it (`WS-02`) before `WS-05` finalizes
  `schemas/batch/plan.schema.json`.
- `common/transactions.py`, `common/validation.py`, `common/results.py`,
  `common/serialization.py`, `common/limits.py` — existing compact-result and
  limit conventions. Response-detail levels should be informed by these.
- `Docs/BENCHMARK_REPORT.md` + `benchmark_ab_live.json` — an existing A/B
  measurement harness. Our round-trip-reduction metric (§19) should extend this
  rather than start a new one, so numbers stay comparable.
- `Optional/UnrealWatchMCP` — modal-dialog/lockup detection. Directly relevant to
  reliability and long-running jobs.

---

## 7. What is NOT established

Everything here is an open question, not a gap in the file. Do not fill these in
from memory — they are assigned in `research/`.

1. ~~Whether `ModelContextProtocol` supports stdio, or HTTP only.~~ **Closed by RB-04:** HTTP/SSE only; no stdio. See §1.1a / `ADR-0009`.
2. ~~Whether progress / cancellation / job IDs exist for long tool calls.~~ **Closed by RB-04:** heartbeat progress; MCP cancel unwired to ToolsetRegistry; no engine job IDs — UEREMCP poll model (`ADR-0009`).
3. What authentication, if any, the HTTP server performs beyond the Origin localhost
   guard noted in §1.1a. Full bind/exposure check remains (`RB-13`).
4. Whether `RunOnMainThread.h` and other `Private` ToolsetRegistry headers are
   usable from an out-of-tree plugin, or need a public equivalent. (`RB-03`)
5. Exact tool inventory, schemas, and result shapes of each Epic toolset —
   especially `NiagaraToolsets`, `GASToolsets`, `BlueprintTools`/`BlueprintNodeTools`,
   `MaterialTools`. Directory names are confirmed; **tool names quoted from
   REAgentTools docs are not**. (`RB-02`, and each domain brief)
6. Whether Blueprint/Niagara/Material/AnimBP/ControlRig graphs can be fully
   serialized and rebuilt via public editor API, and where that breaks. This is
   the project's central technical risk. (`RB-05`, `RB-07`, `RB-08`, `RB-09`)
7. How `FileSandbox` interacts with package saves, asset registry state,
   Blueprint compilation, and the editor transaction buffer. (`RB-06`)
8. Whether `UAgentSkill` is a suitable base for structured templates. (`RB-10`)
9. Whether UE 5.8 is the shipping/GA version in use and how stable these
   Experimental APIs are across hotfixes. All three plugins are
   `IsExperimentalVersion: true` and two are `NoRedist`. (`RB-01`)
10. Engine source availability beyond installed headers — installed-build `.cpp`
    for these plugins may be absent, limiting how deeply behaviour can be read.
    (`RB-01`)

---

## 8. Consequences you must not re-litigate

Derived directly from the above and locked in ADRs:

1. **We do not write a new MCP transport.** (`ADR-0002`)
2. **We do not rebuild main-thread dispatch, JSON-Schema generation, property
   reflection, tool-call exception handling, or tool search.** (`ADR-0002`)
3. **We do not rebuild file-level rollback before evaluating `FileSandbox`.** (`ADR-0005`)
4. **We do not add a primitive that duplicates an existing Epic toolset tool
   without an audited justification in `docs/audit/`.** (`AGENTS.md`)
5. **Goal-level tools are `AICallable` statics on `UToolsetDefinition` subclasses,
   taking one JSON envelope and returning one JSON envelope.** (`ADR-0002`, `ADR-0003`)
