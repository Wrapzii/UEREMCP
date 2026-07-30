# WS-06: MutatingDispatch adoption (orch API)

- **From:** WS-06
- **Date:** 2026-07-30 (updated post-orch review)
- **Status:** **Prepared** — gate wired; Core dispatch **disabled** until orch merge

## Orch API (not RunOnGameThread)

`ws-01-orch` ships `FUeremcpMutatingDispatch` in **UeremcpCore** (owner WS-03):

| Method | Role |
|---|---|
| `TryBegin(RequestJson, bTargetExists, PredictedDeletedAssetCount, bReadOnlyOperation, OutBlockingJson)` | Permission + path + mutator queue; returns false with blocking JSON |
| `Complete(Response)` | Audit append + mutator release + serialize |
| `IsEffectiveDryRun()` | Post-gate dry_run after ADR-0010 destructive override |

Reference adoption: `UeremcpGameplayToolset::CreateSpell` on `ws-01-orch`.

**Not** a game-thread hop — main-thread assumption remains in Epic bridge polling.

## WS-06 landing (this branch)

| File | Role |
|---|---|
| `FUeremcpBlueprintMutatingGate` | Domain adapter; `#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH` |
| `UeremcpBlueprintToolset::ReadGraph` | `TryBeginRead` → work → `Complete` |
| `UeremcpBlueprintToolset::SubmitGraph` | `TryBeginMutating` before live `ReplaceGraph`; `Complete` on live paths |
| `UeremcpBlueprint.Build.cs` | `UEREMCP_BLUEPRINT_MUTATING_DISPATCH=0` (default) |

### Enable after orch merge

1. Merge `ws-01-orch` (or equivalent) into this worktree.
2. In `UeremcpBlueprint.Build.cs`:
   - Set `UEREMCP_BLUEPRINT_MUTATING_DISPATCH=1`
   - Add `UeremcpCore`, `UeremcpSecurity` to `PrivateDependencyModuleNames`
3. Rebuild plugin; WS-11 adds editor regression for queue + permission blocks.

### Call-site parameters

| Tool | TryBegin | Notes |
|---|---|---|
| `read_graph` | `bTargetExists=true`, `PredictedDeleted=0`, `bReadOnly=true` | Read tier; no mutator slot |
| `submit_graph` (live) | `bTargetExists=true`, `PredictedDeleted=0`, `bReadOnly=false` | Skipped when `options.dry_run` |
| `submit_graph` (dry_run) | *(skipped)* | Matches Gameplay pattern |

Live success adds check `core_mutating_dispatch_admitted`.

## Non-goals

- WS-06 does **not** edit `UeremcpCore/**` or `UeremcpSecurity/**`
- No A6 editor-green claim until WS-11 verifies post-merge
- No RE junction retarget

## Blockers

| Blocker | Owner |
|---|---|
| Orch merge into `ws-06-blueprint` | WS-01 orchestration |
| Flip `UEREMCP_BLUEPRINT_MUTATING_DISPATCH=1` | WS-06 after merge |
| Editor queue/permission regression | WS-11 |
