# WS-03 handoff: publish execute_plan capability

**From:** WS-03  
**To:** WS-01  
**Requested change:** Update `execute_plan` in `docs/CAPABILITY_CATALOG.md`
from `planned` to `partial` / available with explicit limitations.

## Landed agent-facing path

`UUeremcpReferenceToolset` now exposes:

```cpp
UFUNCTION(meta = (AICallable), Category = "UEREMCP")
static FString ExecutePlan(const FString& RequestJson);
```

The wrapper delegates directly to
`FUeremcpPlanActions::ExecutePlan(RequestJson)` without parsing or changing the
request `[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpReferenceToolset.cpp]`.

The reference toolset class is registered with `UToolsetRegistry` after engine
initialization
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpCoreModule.cpp]`.
Core's schema-capture automation now requires an `ExecutePlan` tool with a
string `requestJson` property
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/Tests/UeremcpReferenceToolsetTests.cpp]`.

## Why `partial`

The Protocol adapter and its fake-handler automation exist, and the Core
`AICallable` wrapper is registered. End-to-end editor reachability and a domain
handler smoke test still require the shared RE orchestration environment. This
handoff does not claim POC-B.

## Remaining orchestration verification

1. Build the RE editor with UEREMCP enabled.
2. Confirm MCP `list_toolsets` / schema exposes `ExecutePlan`.
3. Run:
   `pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Protocol.PlanActions"`
4. Dry-run a one-operation plan against a registered domain handler.
5. Confirm Templates `instantiate_template` still reaches the same executor.
