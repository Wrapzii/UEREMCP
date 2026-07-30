# WS-06: Mutating main-thread dispatch adoption (proposal)

- **From:** WS-06
- **Date:** 2026-07-30
- **Status:** Proposal only — **UeremcpCore wiring not owned by WS-06**

## Problem

`UUeremcpBlueprintToolset::ReadGraph` / `SubmitGraph` execute synchronously on
whatever thread invokes the MCP tool. Epic `ToolsetRegistry::ExecuteTool` and
Blueprint graph mutation require the **game/editor main thread**. Today the
toolset calls Epic bridge + `WriteGraphDsl` inline; this is fragile if transport
ever invokes tools off-thread (WS-04) or during async job polling (ADR-0009).

There is **no** `MutatingDispatch` helper in the repository yet (searched
`Plugins/UEREMCP/Source/**`, 2026-07-30).

## Ask (WS-03 / UeremcpCore)

Introduce a shared helper (name illustrative):

```cpp
// UeremcpMutatingDispatch.h — owner: WS-03
template<typename TResult>
TResult RunOnGameThread(TFunction<TResult()> MutatingWork);
```

Requirements:

1. **Block caller** until main-thread work completes (matches current sync tool shape).
2. **Propagate errors** as `TResult` or `TOptional` + `FString Error` — do not swallow.
3. **Metrics hook** — increment `InternalOperations` once per dispatched mutation batch.
4. **Reentrant-safe** — if already on game thread, run inline (no deadlock).

## WS-06 adoption plan (after WS-03 lands)

| Call site | Wrap |
|---|---|
| `FUeremcpBlueprintEpicBridge::ExecuteToolSync` | Entire poll loop |
| `FUeremcpBlueprintGraphWriter::ReplaceGraph` | `WriteGraphDsl` + save + re-read |
| `FUeremcpBlueprintGraphReader::ReadGraph` | Graph walk (if ever off-thread) |

Blueprint toolset surface stays JSON-in/JSON-out; dispatch is internal.

## Non-goals

- WS-06 does **not** edit `UeremcpCore/**` (WS-03 owned)
- No claim that editor tests are green (A6) after dispatch — verification remains WS-11
- No async fire-and-forget; ADR-0009 jobs still poll `get_job_result`

## Handoff

| Owner | Action |
|---|---|
| **WS-03** | Implement `RunOnGameThread` (or Epic-equivalent) in `UeremcpCore` |
| **WS-06** | Replace inline Epic calls with dispatch wrapper in one follow-up commit |
| **WS-11** | Add regression test: invoke tool from non-game thread stub → no ensure |

## POC impact

None until wired. Offline tests and scratch-path policy remain valid without dispatch.
