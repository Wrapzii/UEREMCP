# Capability Matrix — Epic Engine Toolsets (UE 5.8)

- **Owner:** WS-02
- **Status:** **SEED ONLY — not started.** Structure and verified directory-level facts
  are recorded; the per-tool inventory is the work.
- **Source:** directory inspection of `$TS` on 2026-07-29. **No runtime enumeration yet.**
- **Brief:** [RB-02](../research/RB-02-epic-toolset-inventory.md)

> **Read this first:** everything below the horizontal rule is a *placeholder awaiting
> RB-02*. Do not cite this file as evidence of what Epic tools do until it is filled in
> from runtime enumeration.

## Verified: the toolsets exist

27 directories under
`$UE_ROOT/Engine/Plugins/Experimental/Toolsets/`
`[VERIFIED: directory listing 2026-07-29]`:

| Toolset | Has `Source/` | Enabled in `RE.uproject` | Domain relevance |
|---|---|---|---|
| `AIModuleToolset` | yes | **yes** | WS-09 (AI/behaviour) |
| `AllToolsets` | aggregator | **yes** | **May pull in others — check load state** |
| `AnimationAssistantToolset` | yes | **yes** | WS-10 |
| `AutomationTestToolset` | yes | **yes** | WS-11 — possible test harness |
| `ChaosClothAssetToolset` | yes | **yes** | low |
| `ConfigSettingsToolset` | yes | **yes** | WS-12 |
| `ConversationToolset` | yes | **yes** | unknown — investigate |
| `DataRegistryToolset` | yes | not listed | WS-09 (data assets) |
| `DataflowAgent` | yes | not listed | unknown — investigate |
| `EditorToolset` | yes | **yes** | general |
| `GASToolsets` | yes | not listed | **WS-09 — high relevance** |
| `GameFeaturesToolset` | yes | not listed | medium |
| `GameplayTagsToolset` | yes | not listed | **WS-09 — high relevance** |
| `LiveCodingToolset` | yes | **yes** | WS-03 iteration |
| `MCPClientToolset` | yes | **yes** | **RB-11 — editor as MCP client?** |
| `MVVMToolset` | yes | **yes** | UI |
| `MetaHumanGenerator` | yes | **yes** | low |
| `NiagaraToolsets` | yes | not listed | **WS-07 — highest relevance** |
| `PCGToolset` | yes | not listed | later domains |
| `PhysicsToolsets` | yes | not listed | medium |
| `PluginToolset` | yes | not listed | low |
| `SemanticSearchToolset` | yes | not listed | **WS-15 — template similarity search?** |
| `SequencerAnimMixerToolset` | yes | **yes** | WS-10 |
| `SlateInspectorToolset` | yes | not listed | WS-12 (dialog detection) |
| `StateTreeToolset` | yes | not listed | later domains |
| `UMGToolSet` | yes | not listed | later domains |
| `WorldConditionsToolset` | yes | not listed | low |

Plus, from `$TR` itself, `UAgentSkillToolset` — the in-engine reference implementation of
an `AICallable` toolset, with `ListSkills` / `GetSkills` / `CreateSkill` / `UpdateSkill`
`[VERIFIED: $TR/.../Public/ToolsetRegistry/AgentSkill.h]`.

### Immediate observation

**"Enabled in `RE.uproject`" is not "loaded at runtime."** Several high-relevance
toolsets — `NiagaraToolsets`, `GASToolsets`, `GameplayTagsToolset`,
`SemanticSearchToolset` — are **not** listed in the uproject, yet REAgentTools'
documentation refers to `NiagaraToolsets.*` as available `[UNVERIFIED]`. Either
`AllToolsets` pulls them in, or that documentation is stale. **RB-02 question 1 resolves
this and should be answered on day one** — several workstreams are planning around tools
that may or may not be reachable.

---

## Matrix — TO BE FILLED BY RB-02

One row per tool. See `_TEMPLATE-capability-matrix.md` for column meanings.

| Toolset | Tool | Purpose | Input | Output | Limitations | Altitude | Disposition | Superseded by | Tag |
|---|---|---|---|---|---|---|---|---|---|
| _empty_ | | | | | | | | | |

## Do-not-rebuild list — TO BE FILLED

## Real gaps — TO BE FILLED

## Coverage assertion — TO BE FILLED

---

## Unverified claims to check, not propagate

These tool names circulate in REAgentTools' documentation. They are recorded here **as
things to verify**, and must not be cited as facts until RB-02 confirms them
`[UNVERIFIED — source: $RAT/Docs/CAPABILITY_MATRIX.md and NIAGARA_BATCHING.md]`:

- `ProgrammaticToolset.execute_tool_script` — **highest value to confirm.** Possibly an
  existing batching primitive that `execute_plan` should compose rather than replace.
  WS-05 is blocked on this.
- `BlueprintTools` / `BlueprintNodeTools` — **if these already author graph nodes,
  RB-05's scope shrinks and R-01's severity drops.** Second-highest value to confirm.
- `NiagaraToolsets.*` — system/emitter/renderer authoring
- `MaterialTools` — master material graph editing
- `ObjectTools.get_properties`, `ActorTools.set_actor_transform`, `SceneTools`
- `LogsToolset.GetLogEntries`, `LiveCodingToolset`, `SlateInspector`
