# Proposal: WS-02 audit rows — template-related Epic tools

- **From:** WS-15
- **To:** WS-02
- **Date:** 2026-07-29

Rows for `docs/audit/epic-toolsets.md` when RB-02 fills the matrix.

| Toolset | Tool | Purpose | Input | Output | Limitations | Altitude | Disposition | Superseded by | Tag |
|---|---|---|---|---|---|---|---|---|---|
| `UAgentSkillToolset` | `ListSkills` | Enumerate all `UAgentSkill` subclasses with non-empty description | none | `TMap<path, description>` | No search; no structured payload; path filtering via settings | primitive | **preserve** | — | `[VERIFIED: AgentSkill.cpp:26-57]` |
| `UAgentSkillToolset` | `GetSkills` | Load skill details | `SkillPaths[]` | `TMap<path, FAgentSkillDetails>` | Details = `Instructions` string only | primitive | **preserve** | — | `[VERIFIED: AgentSkill.cpp:60-85]` |
| `UAgentSkillToolset` | `CreateSkill` | Create Blueprint-backed skill | folder, name, description, details | class path | Permission-gated in docs; folder must exist | composite | **preserve** | — | `[VERIFIED: AgentSkill.cpp:88-130]` |
| `UAgentSkillToolset` | `UpdateSkill` | Update Blueprint-backed skill | path, description, details | bool | Refuses native/transient classes | composite | **preserve** | — | `[VERIFIED: AgentSkill.cpp:133-168]` |
| `SemanticSearchToolset` | `Search` | NL hybrid search over indexed assets | query, class filter, path regexes, K | async results | JSON templates not indexed; embedding provider required | composite | **compose** | `search_templates` for patterns | `[VERIFIED: SemanticSearchToolset.h:55-60]` |
| `SemanticSearchToolset` | `FindSimilar` | Vector similarity from reference asset | asset path, filters, K | async results | Source must be indexed | composite | **compose** | — | `[VERIFIED: SemanticSearchToolset.h:74-79]` |

**Real gap (UEREMCP fills):** execution-shaped template store with `construction_plan`,
versioning, inheritance, modifier grammar, promotion, validation rules, failure
recording. Neither Epic toolset provides this.
