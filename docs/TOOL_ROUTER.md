# Tool Router — intent in, tools + schemas + order out

**Status (2026-07-30):** Production bootstrap tools live on
`UeremcpCore.UeremcpReferenceToolset`:

| Tool | Action | Role |
|---|---|---|
| `GetStarted` | `get_started` | START HERE briefing |
| `ResolveIntent` | `resolve_intent` | Plain-text → ordered plan |
| `DescribeOperation` | `describe_operation` | One-tool schema + example |

Offline twin / CI: [`tools/intent_router/router.py`](../tools/intent_router/router.py).
Historical prototype record: [`docs/TOOL_ROUTER.md`](TOOL_ROUTER.md) (this file) +
[`docs/research/RB-03-intent-router-prototype-provenance.md`](research/RB-03-intent-router-prototype-provenance.md).
Opus prototype preserved at `tools/route_prototype.py`.

---

## 1. The problem, measured

An agent's first two calls accomplish nothing:

| Call | Cost | Outcome |
|---|---|---|
| `list_toolsets` | 17 KB / **~4,270 tokens** | 77 toolset names |
| `describe_toolset` | 0.6–29 KB | one toolset's tools |
| `call_tool` | — | usually rejected; envelope shape unknown |

Measured cost from cold start to **one** successful dry-run call: **~5–10 MCP round
trips** plus reading schema files. The surface is **~911 tools across ~77 toolsets**.

## 2. The shape

```
ResolveIntent("make a spell effect with a helix, and show me what it looks like")
  -> { intent, confidence, confidence_reason, registry_hash,
       plan: [ {step, qualified, request_json, input_schema, safety, …} ],
       alternatives, clarification_questions? }
```

## 3. Deterministic floor

- Candidates only from live `UToolsetRegistry::GetAllToolsetJsonSchemas()`
  `[VERIFIED: $TR/.../UToolsetRegistry.h:54-56]` (offline: `registry_snapshot.json`).
- Structurally cannot emit a tool name absent from the current registry.
- SUPERSEDED redirects (demote ×0.35 + warning) rather than unsafe global hiding.
- Abstain on low confidence or `expected_registry_hash` mismatch.
- `execute_if_complete` is **not** auto-executed in this build (documented).

## 4. describe_toolset description mechanism

Tool descriptions shown by `describe_toolset` are harvested from **UFUNCTION doc
comments** (and class tooltips) via editor JSON-schema metadata:

- `[VERIFIED: Engine/Source/Editor/JsonUtilitiesEditor/.../JsonSchemaGeneratorEditor.h:66-75]`
- Toolset description: `UClass::GetToolTipText()`
  `[VERIFIED: $TR/.../FunctionLibraryToolset.h:42-50]`

Improving agent vocabulary means improving those comments — not markdown alone.

## 5. Ordering

Ordering derives from `tools/intent_router/operation_catalog.json` dependency
metadata (`depends_on_actions`), validated for cycles in CI. Hand-maintained
`DOMAIN_ORDER` is retired from the production path.

## 6. Metrics discipline

**Routing accuracy ≠ end-to-end completion.** Report separately:

- Routing: top-1 / top-3 / MRR / confident-wrong / abstention accuracy
- E2E: whether the agent completed the goal with verified statuses

Baseline EVAL = Opus original 7 prompts — do **not** tune aliases to force 7/7.
Held-out set: `tests/intent_router/heldout_intents.json` (independently authored).

## 7. Focus / SetNameFilters

`FToolset::SetNameFilters` exists
`[VERIFIED: $TR/.../Toolset.h:59-60]` but UEREMCP does **not** apply global hides.
The router demotes SUPERSEDED; focus-mode block lists remain optional and risky.
Keep `list_toolsets` reachable.

## 8. Prototype history

The offline prototype (`tools/route_prototype.py`) measured top-1 **2/7** and top-3
**4/7** against architecture-vocabulary descriptions. Production adds catalog
enrichment + improved UFUNCTION vocabulary + live registry binding. See readiness
proposal for measured rates after land.
