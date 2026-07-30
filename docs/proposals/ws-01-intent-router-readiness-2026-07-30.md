# WS-01 readiness: dynamic plain-text intent router (2026-07-30)

**Status:** Implemented on branch `ws-03-intent-router` from main `4df4f05`.  
**Owners touched:** WS-03 (Core tools), WS-05 (specification schemas), WS-13 (guides), WS-01 (this note / catalog), plus domain UFUNCTION vocabulary (Niagara/Material/Blueprint/Validation) required for live `describe_toolset`.

## Architecture

```
plain text intent
    → UeremcpCore.UeremcpReferenceToolset.ResolveIntent
    → live UToolsetRegistry::GetAllToolsetJsonSchemas()
       [VERIFIED: $TR/.../UToolsetRegistry.h:54-56]
    → BM25-ish lexical rank + operation_catalog enrichment
    → ordered plan with only registry-present qualified names
```

Bootstrap surface:

| Tool | Action |
|---|---|
| `GetStarted` | `get_started` |
| `ResolveIntent` | `resolve_intent` |
| `DescribeOperation` | `describe_operation` |

Schemas (specification only, ADR-0003): `schemas/domains/_shared/{get_started,resolve_intent,describe_operation}.schema.json`.

Offline twin: `tools/intent_router/router.py` + CI `tests/intent_router/`.

### Deterministic floor

- Impossible to emit a tool name absent from the current registry/snapshot.
- SUPERSEDED demotion (×0.35 + warning), not global `SetNameFilters` hides.
  `SetNameFilters` exists `[VERIFIED: Toolset.h:59-60]` but is **not** applied.
- Abstain on registry hash mismatch or weak non-UEREMCP signal.
- `execute_if_complete` is **recommend-only** in this build (cannot safely verify complete fields from plain text alone).

### describe_toolset mechanism

Tool descriptions come from UFUNCTION doc comments via editor JSON schema metadata:

- `[VERIFIED: JsonSchemaGeneratorEditor.h:66-75]`
- Toolset tooltip: `[VERIFIED: FunctionLibraryToolset.h:42-50]`

## Metrics (offline twin vs frozen `registry_snapshot.json`)

**Epistemic constraint:** routing accuracy ≠ end-to-end completion.

### Baseline (Opus original 7 — aliases not tuned to force 7/7)

| Metric | Prototype (pre) | Production twin (post) |
|---|---|---|
| top-1 | 2/7 | **4/7** |
| top-3 | 4/7 | **5/7** |

Misses remain honest (e.g. montage authoring expectation vs inspect-only; log tool naming drift).

### Held-out (independently authored, 18 intents)

| Metric | Value |
|---|---|
| top-1 (routable) | 9/14 (64.3%) |
| top-3 (routable) | 12/14 (85.7%) |
| MRR | 0.625 |
| confident-wrong | 2 |
| abstention accuracy | **4/4 (100%)** |

### Contract fixtures

`tests/intent_router/test_intent_router_contract.py`: stale hash abstain, adversarial bogus names, no absent names, dependency cycle check — **target 100% on these only**.

### End-to-end / fresh-agent

Recorded separately after live deploy:

| Metric | Previous | Target / measured |
|---|---|---|
| Calls to first successful dry-run | ~10 | Expect ≤3 with ResolveIntent (GetStarted→ResolveIntent→domain call) |
| E2E scratch workflow | — | Pending live editor validation after build |

## Provenance

Opus prototype snapshotted from dirty root without modifying it:

- See `docs/research/RB-03-intent-router-prototype-provenance.md`
- Historical prototype kept at `tools/route_prototype.py`

## Limits

- Lexical ranking can still miss paraphrases; abstention is preferred over confident-wrong.
- Snapshot used for offline CI may lag live registry until `dump_tool_registry.py` reruns.
- Domain UFUNCTION vocabulary updates require rebuild for `describe_toolset` to reflect them.
- Capture may route to RECapture when VisualCapture is absent from a given snapshot.
- No claim of universal 100% routing.

## Focus / discovery

Docs and class tooltips mark **START HERE → GetStarted / ResolveIntent**. Focus-mode block lists remain optional; router demotion is the safe default.
