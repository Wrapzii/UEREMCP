# WS-06 response: WS-10 UEdGraph share proposal (`c49cf59`)

- **From:** WS-06
- **To:** WS-10
- **Re:** `docs/proposals/ws-10-edgraph-share-ws06.md`
- **Date:** 2026-07-30
- **Decision:** **Option A accepted** — shared read-only reader landed in `UeremcpBlueprint`.

## Shared API (WS-10 integration)

| Symbol | Header | Role |
|---|---|---|
| `FUeremcpEdGraphReader::ReadGraph` | `UeremcpEdGraphReader.h` | UEdGraph → `graph.schema.json` nodes/links/diagnostics |
| `FUeremcpEdGraphReadOptions` | same | `GraphType`, `bEmitNodesAndLinks`, fidelity/lossy |
| `FUeremcpEdGraphSemanticHooks` | same | Family `semantic_type` / `semantic_id` / entry nodes |
| `FUeremcpEdGraphReader::MakeNodeId` | same | Retrieval-local `n<guid-digits>` (not contract identity) |
| `FUeremcpEdGraphReader::MakePinId` | same | Retrieval-local pin id |
| `FUeremcpEdGraphReader::PinTypeToJson` | same | Structured `FEdGraphPinType` JSON |

### WS-10 dependency

Add to `UeremcpAnimation.Build.cs`:

```csharp
PrivateDependencyModuleNames.Add("UeremcpBlueprint");
```

Include:

```cpp
#include "UeremcpEdGraphReader.h"
```

### Example (AnimBP EventGraph — K2 family)

```cpp
FUeremcpEdGraphSemanticHooks Hooks;
Hooks.ResolveSemanticType = /* map UAnimGraphNode_* / K2 */;
Hooks.ResolveSemanticId = /* WS-10 semantic_id scheme */;
Hooks.IsEntryNode = /* state machine / anim entry policy */;
Hooks.GatherEntryNodes = /* optional */;
Hooks.IsExecPin = /* default K2 exec ok for EventGraph */;

FUeremcpEdGraphReadOptions Opts;
Opts.AssetPath = AssetPath;
Opts.GraphName = Graph->GetName();
Opts.GraphType = TEXT("AnimBlueprintGraph"); // or AnimStateMachine
Opts.bRoundTripSupported = false;
Opts.LossyAreas = { TEXT("anim_state_machine_authoring_unsupported"), ... };

FUeremcpEdGraphReadResult Result;
if (!FUeremcpEdGraphReader::ReadGraph(Graph, Opts, Hooks, Result)) { /* error */ }
// Attach extensions.animation on Result.Graph; set content_hash via FUeremcpContentHash
```

## Ownership split (confirmed)

| Layer | Owner |
|---|---|
| Generic UEdGraph walk | WS-06 (`FUeremcpEdGraphReader`) |
| Blueprint K2 semantic_id | WS-06 (`FUeremcpBlueprintGraphReader` hooks) |
| AnimBP semantic mapping + `extensions.animation` | WS-10 |
| Promote to neutral module (`UeremcpGraph`?) | WS-01 proposal if Animation→Blueprint dep is undesirable long-term |

## Blueprint refactor

`FUeremcpBlueprintGraphReader::ReadGraph` now delegates node/pin/link emission to
`FUeremcpEdGraphReader` and retains Blueprint-only layers: K2 semantic ids, variables,
DSL extension, content hash.

## Not in this slice

- WS-10 AnimGraph semantic hooks (WS-10 owned)
- Promoted shared module (needs WS-01 if required)
- Editor/runtime proof (WS-11 post-orch)
- A6 editor-green — **not claimed**

## Sequencing (agreed)

1. WS-10: depend on `UeremcpBlueprint`, consume reader for AnimBP **EventGraph** first.
2. WS-10: add AnimGraph / state-machine hooks + `extensions.animation`.
3. WS-01 (optional): extract `FUeremcpEdGraphReader` to neutral module if deps grow.
