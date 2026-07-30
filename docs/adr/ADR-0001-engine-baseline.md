# ADR-0001: Engine baseline and substrate

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** all workstreams
- **Depends on:** direct inspection of the local engine install

## Context

The master prompt sets UE 5.8 as the target "unless project inspection determines
otherwise." Project inspection was performed.

`RE.uproject` declares `"EngineAssociation": "5.8"`
`[VERIFIED: $UEREMCP_LEGACY_PROJECT/RE.uproject]`. Two
engines are installed — `UE_5.4` and `UE_5.8`
`[VERIFIED: $UE_INSTALL_ROOT/]`. UE 5.8 is confirmed as the baseline.

More consequentially, inspection found that UE 5.8 ships an agent-facing stack Epic
built for this exact purpose, which the master prompt was written without knowledge of:

- `Engine/Plugins/Experimental/ModelContextProtocol` — "Anthropic MCP (Model Context
  Protocol) server implementation for Unreal Engine," by Epic Games
  `[VERIFIED: $MCP/ModelContextProtocol.uplugin]`
- `Engine/Plugins/Experimental/ToolsetRegistry` — tool registration, JSON Schema
  generation, async tool results, property reflection, file sandboxing
  `[VERIFIED: $TR/ToolsetRegistry.uplugin]`
- `Engine/Plugins/Experimental/Toolsets` — 27 domain toolsets
  `[VERIFIED: directory listing of $TS]`

Full detail in `docs/GROUNDED_FACTS.md`.

This changes the project's shape. The master prompt's premise — that we build a new
MCP against "the currently available Unreal MCP implementation" — is accurate, but
that existing implementation is *Epic's own, in-engine*, not a third-party plugin.
The gap to close is not transport or tool plumbing; it is semantic altitude,
graph round-trip, validation, batching, and reusable patterns.

## Decision

We will target **UE 5.8** and build **on top of** Epic's `ModelContextProtocol` and
`ToolsetRegistry` plugins, treating them as the substrate.

Concretely:

1. `ModelContextProtocol` is our MCP transport, session layer, and capability
   negotiation. We do not reimplement it.
2. `ToolsetRegistry` is our tool declaration, schema-generation, async-result, and
   main-thread-dispatch layer. We do not reimplement it.
3. Epic's 27 domain toolsets are treated as **available primitives** that our
   goal-level operations compose, not as competitors to replace.
4. All three plugins are `IsExperimentalVersion: true`, and `ModelContextProtocol`
   and `ToolsetRegistry` are additionally `NoRedist`
   `[VERIFIED: both .uplugin files]`. We accept experimental-API churn risk. It is
   logged in `docs/RISK_REGISTER.md` as R-01 and R-02.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Target UE 5.4 (also installed) for API stability | The project is on 5.8; 5.4 predates this entire agent stack. Would mean building everything from scratch. |
| Ignore Epic's plugins, build fully independent | Reimplements a working MCP server, schema generator, sandbox, and 27 toolsets. Enormous cost, worse result, guarantees drift from engine direction. |
| Fork Epic's plugins into the project | `NoRedist` and experimental; forking means inheriting maintenance of engine code and losing hotfix improvements. Revisit only if an out-of-tree plugin cannot reach the needed API (see `RB-03`). |
| Wait for these APIs to leave Experimental | No stated timeline. The user needs this now. |

## Consequences

**Enables:** we start from a working MCP server, JSON-Schema generation, async tool
calls, main-thread marshalling, file sandboxing, and broad domain coverage on day
one. Effort concentrates on the actual differentiator.

**Costs:** we are coupled to experimental, `NoRedist` engine APIs that can change in
any 5.8 hotfix or in 5.9. Every engine update is a regression-test event. Mitigations:
pin the engine version in CI, keep an adapter layer between our domain services and
`ToolsetRegistry` types so churn is absorbed in one place, and keep the
integration-test suite (`WS-11`) fast enough to run on every engine bump.

**Locks in:** editor-only, Windows-first, in-editor execution. Headless/commandlet
operation is not assumed and must be researched separately if wanted.

## Open questions

- Is UE 5.8 GA or preview on this machine, and what is the hotfix cadence? (`RB-01`)
- Is engine `.cpp` source available, or only installed headers? This bounds how
  deeply behaviour can be verified rather than inferred. (`RB-01`)
- Which of the 27 toolsets are actually loaded in the RE project at runtime?
  `RE.uproject` lists only some, but `AllToolsets` may pull in more. (`RB-02`)

## Verification

An implementer complies if their plugin builds against UE 5.8, declares
`ToolsetRegistry` (and where needed `ModelContextProtocol`) as plugin dependencies,
and adds no module that duplicates a listed engine module.
