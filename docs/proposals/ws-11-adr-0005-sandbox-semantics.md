# Proposal: ADR-0005 sandbox semantics — supplement, do not replace (WS-11)

- **From:** WS-11
- **To:** WS-01 (owns ADRs)
- **Date:** 2026-07-29
- **Related:** ADR-0005, RB-06
- **Status:** C-3 closed for Content/ full-Discard on shipping plugin (2026-07-30)

## Verdict on ADR-0005

**Do not replace ADR-0005.** The two-layer design (`FGlobalSandbox` + editor
transactions) still matches the engine surface.

**Engine** `Rollback.MultiAssetDiscard` path is **observed green** (2026-07-29) for
N× Content/`UCurveFloat` creates → Discard+Leave (interim probe; UEREMCP disabled).
Per WS-14 **C-3** (accepted by WS-01), that proves FileSandbox **engine** semantics
only — **not** the shipping UEREMCP plugin gate.

Shipping Validation gate is green `[VERIFIED-RUNTIME: tests/integration/_logs/editor_UEREMCP_Validation_shipping.redacted.md]`. WS-01 may still keep `rollback.available: false` until BP compile / deletion / non-mount paths are gated. Supplement hazards below; do not claim BP compile / deletion /
`Saved|Config` coverage yet.

Full evidence in `docs/research/RB-06-sandbox-and-rollback.md`.

## Evidence summary

### q1 — package-save interception

**POSITIVE** for Content/ mount-point `UPackage::Save`  
`[VERIFIED-RUNTIME: UnrealEditor-Cmd UEREMCP.Validation.Rollback.MultiAssetDiscard 2026-07-29]`.

Source: FileSandbox installs `FSandboxPlatformFile` as the top `IPlatformFile` on
`Initialize` `[VERIFIED: SandboxPlatformFile.cpp:146-164]`. SavePackage uses
`IFileManager` `[VERIFIED: SavePackageUtilities.cpp]`. Tracked paths are **content
mount points only**; `Saved/` and `Config/` are not sandboxed  
`[VERIFIED: ISandboxInstance.h:28-30]`.

### q3 — asset registry / in-memory UObject after Discard

**POSITIVE** for full `Discard()` on added Content packages  
`[VERIFIED-RUNTIME: same test — disk/AR/FindPackage clean; retry create ok]`.

Source: `FGlobalSandbox::Discard` → `RevertAll()` → `PurgePackages` +
`HotReloadPackages` `[VERIFIED: SandboxLibrary.cpp:163-175;
SandboxInstance.cpp:223-228; PackageSandboxUtils.cpp:58-148]`.

**Hazard — DiscardFiles path differs:** `FGlobalSandbox::DiscardFiles` calls
`RevertSandbox` **directly** and does **not** purge/hot-reload  
`[VERIFIED: SandboxLibrary.cpp:178-203]`. Single-file revert is experimental
incomplete `[VERIFIED: ISandboxInstance.h:50-51, UE-368478]`. Prefer full
`Discard()` for failure rollback.

### Nesting (bonus, from Enter source)

`Enter` while a *different* sandbox is active **Leaves** the current one first
(preserving its files — Leave does not Discard)  
`[VERIFIED: SandboxLibrary.cpp:67-81, SandboxLibrary.h:33-38]`. Same-name Enter
is a no-op. Nested concurrent sandboxes are **not** supported; a nested request
must detect `IsActive()` and refuse or serialize — agrees with ADR-0005 rule 3.

## Recommended supplements (for WS-05 batch executor)

1. Shipping Validation gate is green — allow `rollback.available: true` **only**
   for Content/ full-`Discard`. Still degrade for BP compile / deletions /
   non-mount paths until those are gated.
2. Prefer `Discard()` (full) over `DiscardFiles()` for failure rollback, **or**
   after `DiscardFiles` explicitly purge/hot-reload the returned package sets.
3. Derive `changes` from `GetChanges()` as ADR-0005 says, but **supplement**
   `asset_class` / soft package name / revision — `FSandboxedFileChangeInfo`
   only has `Path`, `Action`, `Timestamp`  
   `[VERIFIED: SandboxedFileChangeInfo.h:31-41]`. Mapping draft:
   `tests/unit/test_harness_conventions.py` (`map_sandboxed_change_to_entry`).
4. Do not treat Leave as Discard — Leave preserves sandbox files  
   `[VERIFIED: SandboxLibrary.h:35-38]`.
5. In-memory-only dirty packages are **not** in `HasFileChanges` / GetChanges  
   `[VERIFIED: ISandboxInstance.h:78-81]` — transaction layer remains mandatory.

## Known hazards for WS-12

- `DiscardFiles` without purge/reload → stale UObjects / AR (above).
- `Enter` of a different name silently Leaves the previous sandbox (files kept
  in Intermediate/Sandboxes) — can surprise concurrent agents.
- UndoTransaction can undo user work (RB-06 §C q12 — still untested).
- ForceReplaceReferences on purge is disabled as too aggressive  
  `[VERIFIED: PackageSandboxUtils.cpp:122-128 comment]` — reference cleanup
  may be incomplete for complex assets.
## Response

**Accepted — supplement, do not replace ADR-0005.**

- q1 and q3 closed for the proven scope (Content/ mount `UPackage::Save` adds +
  full `Discard()`), backed by green `Rollback.MultiAssetDiscard` on the
  **shipping** plugin (`bf30d8f` / orch) and earlier probe evidence.
- `rollback.available: true` allowed **only** for that scoped path. Keep false /
  degraded for BP compile/CDO, deletions, and `Saved/`/`Config/` until gated.
- Prefer `Discard()` over `DiscardFiles()` for failure rollback (or purge after
  DiscardFiles).
- ADR-0005 Verification section updated; open questions 4–6 remain open.
- R-03 marked **mitigated** (narrow residual scope).
