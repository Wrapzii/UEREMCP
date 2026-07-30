# ADR-0007: Implementation language — C++ primary, Python exploratory

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** every domain workstream choosing where to put service code
- **Depends on:** ADR-0002, `RB-03`

## Context

ADR-0002 locks the agent-facing host as in-process `UToolsetDefinition` /
`AICallable` toolsets. That does not by itself decide whether domain *services*
behind those toolsets are C++ or Python. REAgentTools today is Python-heavy and
benefits from `_reload.py` hot-reload
`[VERIFIED: ToolsetRegistry Content/Python/toolset_registry/_reload.py per ADR-0002]`.
Master prompt rule 20 rejects Python as the *primary* layer for graph /
compilation / transaction work.

RB-03 closed the mechanics that matter for the split
`[VERIFIED-RUNTIME: docs/research/RB-03-plugin-integration.md]`:

1. Out-of-tree C++ plugins can register `AICallable` toolsets and load when all
   declared modules build (`UeremcpCore` / `Protocol` / `Transport` /
   `Validation` DLLs present).
2. Private ToolsetRegistry headers (`RunOnMainThread.h`, `JsonSchema.h`, …) are
   **not** reachable without hacking include paths; public substitutes exist
   (`Async(EAsyncExecution::TaskGraphMainThread, ...)`,
   `UToolsetRegistry::GetToolsetJsonSchema`, `RegisterToolsetClass`).
3. Live Coding / hot reload **blocks** `Build.bat` while the editor holds modules;
   successful rebuilds need `-NoHotReloadFromIDE` or a closed editor. Changing
   `AICallable` signatures requires UHT + full module rebuild — Live Coding is
   not a signature-iteration path.

## Decision

We will implement UEREMCP as follows:

1. **Agent-facing toolsets and domain services that touch graphs, assets,
   compilation, transactions, FileSandbox, or editor subsystems are C++** in
   `Plugins/UEREMCP/Source/<Module>/`. Toolsets stay thin; services own the work
   (ADR-0002 §4).
2. **Python is allowed as a secondary, exploratory layer** — prototyping
   signatures, glue that only calls already-registered tools (including Epic's
   `ProgrammaticToolset.execute_tool_script`), and temporary probes. It is not
   the shipping path for goal-level operations that must verify compilation or
   mutate graphs.
3. **Frozen production tools do not depend on Live Coding.** Signature and
   module iteration assumes a rebuild with the editor unlocked (or closed).
4. **Do not consume private ToolsetRegistry headers** from out-of-tree modules.
   Use the public APIs listed in RB-03 q10.

Envelope JSON remains `FString` in / `FString` out at the tool boundary
(ADR-0003); language choice does not change that contract.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Python-only agent surface (REAgentTools pattern) | Rejected by master prompt rule 20 and ADR-0002; cannot reliably reach the APIs POC validation needs. |
| C++ only, ban Python entirely | Throws away a real iteration advantage for exploratory glue and Epic's existing script batching. Unnecessary once the primary layer is C++. |
| Patch ToolsetRegistry private includes into plugin Build.cs | Fragile across engine updates; RB-03 shows public equivalents for the Wave 1 needs. |

## Consequences

**Enables:** honest ownership of compile/validation in C++; Python for cheap
exploration without pretending it is the host.

**Costs:** signature iteration is slow (UHT + rebuild). Agents must plan for
editor unlock / `-NoHotReloadFromIDE` (RB-03 q15).

**Locks in:** C++ as the shipping implementation language for domain services
behind `AICallable` toolsets. Reversing that means rewriting those services.

## Open questions

- Whether a thin Python shim that only forwards envelope JSON to C++ services is
  ever useful in production (default: no — prefer direct C++ toolsets).
- Hybrid `USTRUCT` + JSON `specification` for schema richness (ADR-0003 /
  RB-03 q7) — language-independent; tracked under R-04.

## Verification

- Shipping modules link as editor DLLs under `Plugins/UEREMCP/Binaries/Win64/`.
- Reference tools register via public `UToolsetRegistry::RegisterToolsetClass`.
- No plugin module lists private ToolsetRegistry include paths in its Build.cs.
