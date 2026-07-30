# RB-06: `FileSandbox` semantics, transactions, and rollback

- **Owner:** WS-11
- **Status:** q1/q3 answered for **engine** FileSandbox; **shipping UEREMCP gate unproven** (C-3)
- **Blocks:** ADR-0005 confidence, WS-05's batch executor, every atomic-batch claim
- **Priority:** highest — start immediately
- **Updated:** 2026-07-29 (C-3 nuance accepted by WS-01)

## Why this is urgent

ADR-0005 builds all rollback on `UE::ToolsetRegistry::FGlobalSandbox`
(`Enter` / `GetChanges` / `Persist` / `Discard` / `DiscardFiles`)
`[VERIFIED: $TR/.../Public/ToolsetRegistry/SandboxLibrary.h]` plus the editor
transaction buffer via `UToolsetLibrary::UndoTransaction` / `GetActiveUndoCount`
`[VERIFIED: ToolsetLibrary.h]`.

The header tells us the API shape. It does not tell us the **semantics**, and the
semantics are what determine whether atomic multi-asset batching works at all.

Until questions 1 and 3 below are answered **and** `Rollback.MultiAssetDiscard`
is observed green, **no workstream may claim atomic multi-asset rollback works**,
and `rollback.available` reports `false` in every response (ADR-0005 verification).

## Verdicts (2026-07-29)

| Q | Verdict | Confidence | Gate |
|---|---|---|---|
| **q1** package-save interception | **POSITIVE** for Content/ mount-point `UPackage::Save` | Source + `[VERIFIED-RUNTIME: UnrealEditor-Cmd UEREMCP.Validation.Rollback.MultiAssetDiscard 2026-07-29]` | green |
| **q3** AR / UObject after Discard | **POSITIVE** for full `Discard()`→`RevertAll()` on added Content packages | same | green |
| ADR-0005 | **Do not replace.** Supplement hazards (`DiscardFiles`, Saved/Config, BP). Proposal: `docs/proposals/ws-11-adr-0005-sandbox-semantics.md` | — | — |

### C-3 nuance (WS-14 → WS-01 accepted)

| Layer | Status |
|---|---|
| **Engine FileSandbox semantics** (q1/q3 Content/ adds) | **POSITIVE** — observed green 2026-07-29. Probe path used `-DisablePlugins=UEREMCP`. Evidence stays valid as *engine* behaviour. |
| **Shipping UEREMCP plugin gate** | **NOT PROVEN** — test body SoT is `Plugins/.../UeremcpValidation/.../RollbackMultiAssetDiscard.spec.cpp`; requires `UeremcpValidation` in `UEREMCP.uplugin` + loadable `UeremcpCore`, re-run with `-KeepUeremcp -NoProbe`. |

Scope of the engine proof: **N× `UCurveFloat` creates under `/Game/__UeremcpTests/`
inside `FGlobalSandbox`, then Discard+Leave.** Does **not** prove Blueprint
compile/CDO discard, asset deletions, or `Saved/`/`Config/` coverage.

Until the **shipping** gate is green, WS-05 should keep `rollback.available: false`
(or clearly scoped / degraded) for product responses — engine evidence alone is
insufficient per C-3. Prefer `Discard()` over `DiscardFiles` (hazard below).

Interim probe is **launch-smoke only** (no Rollback body). Defaults:
`-DisablePlugins=UEREMCP` + `-EnablePlugins=UeremcpValidationProbe`.

### q1 — Does `FileSandbox` intercept `UPackage::Save`?

**Answer: YES for content mount points.**

1. On sandbox init, `FSandboxPlatformFile::Initialize` installs itself as the top
   platform file via `FPlatformFileManager::Get().SetPlatformFile(*this)`  
   `[VERIFIED: $FS/.../Sandbox/Platform/SandboxPlatformFile.cpp:146-164]`.
2. `UPackage::Save` / SavePackage utilities write through `IFileManager::Get()`  
   `[VERIFIED: $ENGINE/.../SavePackage/SavePackageUtilities.cpp:269,295,462,557-561]`.
3. Coverage is **mount-point content only**. Explicitly: Saved/ and Config/ are not
   affected `[VERIFIED: $FS/.../Public/ISandboxInstance.h:28-30]`.
4. **Runtime:** after Enter + 3× `UPackage::Save` of scratch CurveFloats,
   `GetChanges()` listed matching `__UeremcpTests` / `DiscardAsset_*` paths  
   `[VERIFIED-RUNTIME: editor log Q1 POSITIVE, MultiAssetDiscard Success 2026-07-29]`.

### q3 — Asset registry / in-memory `UObject` after `Discard`?

**Answer: YES for full `Discard()` on added Content packages; HAZARD on `DiscardFiles`.**

1. `FGlobalSandbox::Discard` → `RevertAll()` → `PurgePackages` + `HotReloadPackages`  
   `[VERIFIED: SandboxLibrary.cpp:163-175; SandboxInstance.cpp:223-228;
   PackageSandboxUtils.cpp:58-148]`.
2. **Runtime:** after Discard+Leave, real disk clean, `DoesAssetExist` false,
   AssetRegistry empty for those packages, `FindPackage` null; retry create outside
   sandbox succeeded  
   `[VERIFIED-RUNTIME: editor log Q3 POSITIVE, MultiAssetDiscard Success 2026-07-29]`.
3. **Hazard — `DiscardFiles`:** calls `RevertSandbox` without auto purge/hot-reload  
   `[VERIFIED: SandboxLibrary.cpp:178-203]`. Single-file revert experimental  
   `[VERIFIED: ISandboxInstance.h:50-51, UE-368478]`.

## Questions

### A. Coverage — does the sandbox see what we do?

1. **Does `FileSandbox` intercept Unreal package saves** — see Verdicts / q1.
2. Does it cover asset *deletion*, or only creation and modification? — **open**
3. What happens to the **asset registry** and to in-memory `UObject`s after `Discard`?
   — see Verdicts / q3.
4. How does Blueprint compilation interact? — **open** (not covered by DataAsset gate)
5. Does it cover files outside `Content/` — **NO for Saved/Config** (source above)

### B. Lifecycle

6. **Can sandboxes nest?** `Enter` while a *different* sandbox is active **Leaves**
   the current one first (files preserved — Leave ≠ Discard)  
   `[VERIFIED: SandboxLibrary.cpp:67-81, SandboxLibrary.h:33-38]`. Same-name Enter is
   a no-op. Nested concurrent sandboxes are **not** supported.
7. Leave vs Persist vs Discard — Leave preserves; Discard reverts all and leaves
   sandbox **active**; call Leave after  
   `[VERIFIED: SandboxLibrary.h:34-61]`.
8. Crash with active sandbox — **open**
9. Per-session vs persisted — sandboxes can be resumed by name (`LoadNamedSandbox`)  
   `[VERIFIED: SandboxLibrary.cpp:84-90]` — persistence details **open**
10. `FSandboxedFileChangeInfo` = `Path`, `Action`, `Timestamp` only  
    `[VERIFIED: SandboxedFileChangeInfo.h:31-41]`. Mapping draft in
    `tests/unit/test_harness_conventions.py` — supplement `asset_class` /
    `package_name` / `previous_revision` for envelope `changeEntry`.

### C. Transactions

11–13. Composition with editor transactions / UndoTransaction / BP compile —
    **open** (hazards flagged for WS-12 in the ADR-0005 proposal).

### D. Cost

14–15. Performance / dry_run cost — **open**

## Method

Empirical, in the editor. Suggested sequence:

1. `Enter`, create one asset, `GetChanges()`, inspect. `Discard`. Check disk **and**
   the asset registry **and** the content browser.
2. Repeat with a saved package, a compiled Blueprint, a deleted asset.
3. Attempt a nested `Enter`.
4. Force-fail mid-batch, discard, then attempt the same batch again — does the retry
   succeed cleanly? (This is `Idempotency.RepeatedCreate` interacting with rollback.)

Record exact code and output for every `[VERIFIED-RUNTIME]` claim.

**Automation gate:** `UEREMCP.Validation.Rollback.MultiAssetDiscard`  
(`Plugins/UEREMCP/Source/UeremcpValidation/...` and interim probe under
`tests/integration/editor_plugin/`). Doc: `tests/integration/Rollback.MultiAssetDiscard.md`.

## Deliverables

- [x] Answers to 1 and 3 — source + **engine runtime** for Content/ CurveFloat adds
- [x] Test body SoT in `Plugins/.../UeremcpValidation`; probe collapsed (C-3)
- [~] Engine path observed green (probe, UEREMCP disabled); **shipping** Cmd with
      UEREMCP enabled **blocked** (`UeremcpProtocol` DLL missing — see
      `tests/integration/_logs/shipping-gate-blocker.redacted.md`)
- [x] `FSandboxedFileChangeInfo` → `changeEntry` mapping draft (unit tests)
- [x] ADR-0005 recommendation as proposal (supplement, do not replace; C-3 nuance)
- [x] Known hazards list for WS-12 (in proposal + §C notes above)

## Shipping UEREMCP gate (C-3) � 2026-07-30

`UEREMCP.Validation.Rollback.MultiAssetDiscard` under enabled UEREMCP on `RE.uproject`:
`Result={Success}` `[VERIFIED-RUNTIME: tests/integration/_logs/editor_UEREMCP_Validation_20260729_234458.log]`.
Does not extend coverage beyond CurveFloat Content/ adds documented in the test body.
