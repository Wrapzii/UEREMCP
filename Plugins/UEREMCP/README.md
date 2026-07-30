# UEREMCP plugin — scaffold

> **STATUS: SCAFFOLD. THIS HAS NEVER BEEN COMPILED.**
>
> It was written from a reading of UE 5.8 engine headers
> (`docs/GROUNDED_FACTS.md`), not from a working build. Treat every construct in it as
> `[UNVERIFIED]` until WS-03 makes it build. Expect it to be wrong in at least one
> place — that is what RB-03 is for.

**Owner:** WS-03 (`UeremcpCore`, `.uplugin`), WS-05 (`UeremcpProtocol`).

## What this is for

The smallest artefact that can prove or disprove ADR-0002 — that goal-level operations
can be hosted as static `AICallable` `UFUNCTION`s on `UToolsetDefinition` subclasses,
served through Epic's in-editor MCP server, with no new server process.

Two tools, deliberately trivial:

| Tool | Takes | Proves |
|---|---|---|
| `Ping` | nothing | registration and reachability, isolated from schema questions |
| `Echo` | a request envelope | the full ADR-0003 contract, without touching an asset |

A failure in `Echo` is unambiguously a protocol or registration problem, never a domain
problem. That separation is the point.

## Layout

```
UEREMCP.uplugin              EditorOnly; depends on ToolsetRegistry + ModelContextProtocol
Source/
  UeremcpCore/               WS-03 — toolsets, registration, editor integration
    UeremcpCore.Build.cs
    Public/UeremcpReferenceToolset.h
    Private/UeremcpReferenceToolset.cpp
    Private/UeremcpCoreModule.cpp
  UeremcpProtocol/           WS-05 — envelope, graph, revisions. NO editor dependency.
    UeremcpProtocol.Build.cs
    Public/UeremcpEnvelope.h
```

Domain modules (`UeremcpBlueprint`, `UeremcpNiagara`, `UeremcpMaterial`,
`UeremcpGameplay`, `UeremcpAnimation`, `UeremcpValidation`, `UeremcpSecurity`,
`UeremcpTemplates`) are **not** scaffolded here. Their owners create them — see
`docs/WORK_ALLOCATION.md`.

## Layering rule — do not violate it casually

```
UeremcpProtocol   depends on nothing engine-specific    (unit-testable outside the editor)
       ↑
UeremcpCore       depends on ToolsetRegistry            (the ONLY place that coupling lives)
       ↑
Ueremcp<Domain>   depends on Core + editor subsystems   (the actual work)
```

Domain services must include nothing from `ToolsetRegistry/` or
`ModelContextProtocol/`. Both are Experimental and `NoRedist` — R-02 says they will
churn, and this layering is what makes that survivable (ADR-0001, ADR-0002 rule 4).

## Getting it to build — WS-03's first task

1. Copy or symlink this directory into
   `$UEREMCP_LEGACY_PROJECT/Plugins/UEREMCP`.
2. Enable `UEREMCP` in `RE.uproject`. `ToolsetRegistry` and `ModelContextProtocol`
   must be enabled too — `ModelContextProtocol` is **not** enabled by default
   `[VERIFIED: its .uplugin]`, though it is listed in `RE.uproject` `[VERIFIED]`.
3. Regenerate project files and build.
4. Ensure the MCP server is running — check `bAutoStartServer`, and that the port
   matches `$PROJ/.mcp.json` (`127.0.0.1:8000/mcp`).
5. Connect an MCP client and call `Ping`.

## Known unknowns in this scaffold

Each is an RB-03 question. Do not paper over one silently — record the answer, because
fifteen workstreams are about to depend on it.

| # | Unknown | Where |
|---|---|---|
| 1 | Do `UToolsetDefinition` subclasses self-register, or is `RegisterToolset` required? | `UeremcpCoreModule.cpp` |
| 2 | **What JSON Schema is generated for a single `FString` parameter?** If it is just "a string", ADR-0003 needs revising — this is the one most likely to bite. | `UeremcpReferenceToolset.h` |
| 3 | Is `UEREMCPCORE_API` the right export macro name for this module? | headers |
| 4 | Are `Private` ToolsetRegistry headers reachable out-of-tree? | `UeremcpCore.Build.cs` |
| 5 | Does a `static FString` return work for `AICallable`, or is a `UToolCallAsyncResult` derivative required even for synchronous tools? | `UeremcpReferenceToolset.h` |
| 6 | Does Live Coding handle adding an `AICallable` `UFUNCTION`, or is a restart needed? | — |

`UAgentSkillToolset` is the in-engine reference implementation and answers several of
these by example — read it first
(`$TR/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h`).

## When this works

Update this file to remove the scaffold warning, record the answers above in
`docs/research/RB-03-plugin-integration.md`, and tell WS-01 — ADR-0002 moves from
"grounded in headers" to "proven," and Wave 2 unblocks.
