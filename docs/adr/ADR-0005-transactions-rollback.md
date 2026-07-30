# ADR-0005: Transactions, sandboxing, and rollback

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-05 (batch execution), WS-11 (validation/testing), all domain workstreams
- **Depends on:** ADR-0001, ADR-0003

## Context

Master prompt §2.8 requires multi-asset operations to roll back on failure, tracking
created/modified/deleted assets, previous serialized state, package dirty state,
compilation state, dependencies, and external files.

Two engine mechanisms exist and were verified:

**File-level.** `UE::ToolsetRegistry::FGlobalSandbox` provides static access to a
globally active `FileSandbox` instance
`[VERIFIED: $TR/.../Public/ToolsetRegistry/SandboxLibrary.h]`:

```cpp
static bool IsActive();
static FString GetActiveName();
static bool Enter(const FString& Name, const FString& Description);
static bool Leave();
static TArray<UE::FileSandboxCore::FSandboxedFileChangeInfo> GetChanges();
static bool Persist(const TArray<FString>& Files);
static bool Discard();
static bool DiscardFiles(const TArray<FString>& Files);
```

An `Enter` → mutate → `GetChanges` → `Persist`/`Discard` lifecycle, with per-file
granularity on both persist and discard. `ToolsetRegistry` declares `FileSandbox` as
a plugin dependency `[VERIFIED: $TR/ToolsetRegistry.uplugin]`.

**In-memory.** `UToolsetLibrary::UndoTransaction(bool bCanRedo)` and
`GetActiveUndoCount()` expose the editor transaction buffer
`[VERIFIED: $TR/.../Public/ToolsetRegistry/ToolsetLibrary.h]`.

Neither alone covers the requirement. The transaction buffer does not capture files
already written to disk; the sandbox does not capture in-memory UObject state or
compiled Blueprint bytecode. Multi-asset operations touch both.

## Decision

We will build rollback as a **two-layer composition over engine mechanisms**, and
will not write a bespoke file-versioning or object-snapshot system before those
mechanisms are shown insufficient.

1. **Outer layer — `FGlobalSandbox`.** A batch executing with
   `options.atomic: true` runs inside `FGlobalSandbox::Enter(<request_id>, <summary>)`.
   On success, `GetChanges()` is the authoritative source for the response's
   `changes` manifest and `result.created_assets` / `modified_assets`, and the
   operation ends with `Persist`. On failure with `rollback_on_failure: true`, it ends
   with `Discard`. `partially_completed` uses `Persist(<subset>)` / `DiscardFiles(<subset>)`.
2. **Inner layer — editor transactions.** Individual in-memory mutations are wrapped
   in editor transactions so undo remains coherent for the human at the keyboard.
   `GetActiveUndoCount()` is sampled before and after to detect transaction leaks.
3. **Sandbox state is checked, never assumed.** `IsActive()` and `GetActiveName()` are
   consulted before `Enter`. Nesting behaviour is unverified (see open questions) — a
   nested request must not silently join or clobber an outer sandbox.
4. **The `changes` manifest is derived from `GetChanges()`, not hand-maintained.** A
   manifest assembled by each domain service by hand will drift from what actually
   happened; the sandbox observed it.
5. **`dry_run: true` is implemented as sandbox-enter, execute, report, discard** —
   not as a separate no-op code path. A dry run that exercises different code than
   the real run predicts nothing. This also makes plan-preview genuinely accurate.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Bespoke snapshot/restore of package files before mutation | Reimplements `FileSandbox` with worse editor integration and no engine maintenance. Reconsider only if `RB-06` shows the sandbox does not cover package saves. |
| Editor transaction buffer alone | Does not cover files written to disk, saved packages, or external files. Insufficient for multi-asset. |
| Source control as the rollback mechanism | Requires SCC configured; not all projects have it; slow; conflates agent operations with human commits. May be a *supplementary* safety net — `RB-06`. |
| No rollback; report failure and leave partial state | Directly violates master prompt §2.8 and rule 14 (never silently destroy user content). |
| Serialize every touched UObject before mutation | Very expensive on large batches, and does not solve file-level or compilation state. |

## Consequences

**Enables:** atomic multi-asset batches, accurate change manifests, and dry-run that
exercises the real code path — all from engine-maintained primitives.

**Costs:** we are dependent on `FileSandbox` semantics that are experimental and only
partly documented in headers. If the sandbox does not intercept package saves or
Blueprint compilation artifacts, the outer layer has a hole, and `RB-06` must find it
**before** WS-05 builds `execute_plan` on top. This is sequenced first in the roadmap
for that reason.

**Locks in:** the batch executor's structure around enter/execute/persist-or-discard.

## Open questions

These are the reason `RB-06` is high priority, and none should be assumed:

1. Does `FileSandbox` intercept Unreal package saves (`UPackage::Save`), or only raw
   file writes?
2. Can sandboxes nest? What happens on `Enter` while `IsActive()`?
3. What happens to the **asset registry** and in-memory `UObject`s after `Discard`?
   Are stale objects left pointing at discarded packages, and is an editor restart or
   forced reload required?
4. How does Blueprint compilation interact — is generated bytecode/CDO state
   discarded with the files, or does it survive in memory?
5. Does `Discard` handle assets that were *deleted* during the sandbox, or only
   created and modified?
6. What is the performance cost on a batch touching hundreds of files?

Until (1) and (3) are answered, **no workstream may claim atomic multi-asset rollback
works.** Claim it after `RB-06` and a passing rollback integration test, not before.

## Verification

Integration test `Rollback.MultiAssetDiscard`: create N assets in a sandbox, fail the
last operation deliberately, discard, then assert — no new assets on disk, asset
registry clean, no dangling in-memory references, editor still usable, and a second
identical request succeeds. Until this test passes, `rollback.available` reports
`false` in responses rather than optimistically `true`.
