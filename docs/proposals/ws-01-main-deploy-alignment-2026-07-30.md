# WS-01 main/deploy alignment — 2026-07-30

- **Owner:** WS-01
- **Date:** 2026-07-30
- **Disposition:** local `main` and the RE junction are aligned through the dedicated
  deploy worktree; nothing was pushed. Concurrent agents advanced `main` beyond the
  audit-only tip after the initial deploy; this record documents both steps.

## Before

- Local `main`: `baa0d0663b8488cce6ec76746ae65ffad5fd79eb`.
- Audit branch:
  `ws-01-done-claim-adversarial-audit-2026-07-30` at
  `0099cb3c97e0391d88c21a828b1d7f38bcc1ba99`.
- RE junction:
  `$UEREMCP_LEGACY_PROJECT/Plugins/UEREMCP`.
- Junction target:
  `$UEREMCP_ROOT-ws01/Plugins/UEREMCP`.
- Junction target checkout:
  `b84397fa6ccbe92fe45fd2cdf7b9efd2b6f8aac7`.

## Alignment steps performed by this task

1. Verified `0099cb3` is a fast-forward of local `main`, then advanced `main`
   atomically to `0099cb3`.
2. Created dedicated deploy worktree
   `$UEREMCP_DEPLOY` on branch
   `ws-01-main-deploy-alignment-2026-07-30` at `0099cb3`.
3. Left `UEREMCP-ws01` untouched so concurrent Niagara work could keep using it.
4. Pointed the RE junction at
   `$UEREMCP_DEPLOY/Plugins/UEREMCP`.
5. Recorded the first alignment commit `a2bd21b`
   (`[WS-01] Record main deployment alignment`) and fast-forwarded local `main`
   to that tip.

## Concurrent coordination

The shared RE junction was contested by concurrent agents:

- `ws-07-niagara-inspect-crash-fix` temporarily took the junction for
  `UEREMCP.Niagara.Inspect`, interrupting early rebuild attempts when response
  files vanished under the junction path.
- `ws-11-niagara-visual-tool-validation` later took the junction for VisualCapture
  validation and rebuilds.
- A concurrent full-use integration agent then reused
  `UEREMCP-deploy-main`, switched it onto `ws-01-full-use-integration`, and
  fast-forwarded local `main` through the Niagara inspect crash fix, VisualCapture
  validation, tool-selection contract, and readiness docs.

This task did not merge the 31 historical branches. It also did not call
suspected crashing Niagara tools. After yielding, it rebuilt and live-verified the
advanced local `main` tip that those concurrent agents left behind.

## Rebuild evidence

### First aligned tip (`0099cb3`)

After the Niagara inspect crash-fix automation released the junction, rebuild
against RE succeeded:

```powershell
& "$UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
  UnrealEditor Win64 Development `
  -Project="$UEREMCP_LEGACY_PROJECT\RE.uproject" `
  -WaitMutex -NoHotReloadFromIDE
```

UnrealBuildTool reported `Result: Succeeded` with 98 actions and wrote
`UnrealEditor.target`.
[VERIFIED-RUNTIME: UE 5.8 UnrealBuildTool against RE with junction at
`UEREMCP-deploy-main` SHA `0099cb3`]

### Advanced main tip after concurrent integration

After concurrent agents advanced local `main` through full-use integration, a
second rebuild against the same RE junction succeeded (`Result: Succeeded`,
16 actions / link+metadata for the already-built plugin set).
[VERIFIED-RUNTIME: UE 5.8 UnrealBuildTool against RE with junction at
`UEREMCP-deploy-main` on the advanced local `main` tip that contained the
VisualCapture and Niagara inspect-crash-fix merges]

Plugin source did not change in the later docs-only tip commits
(`cb07277` → `c4fce15` → `90fe0bf`).

## Live toolset evidence

### At audit tip (`0099cb3`)

Read-only `list_toolsets` after the first rebuild returned these seven UEREMCP
toolsets:

1. `UeremcpCore.UeremcpReferenceToolset`
2. `UeremcpTemplates.UeremcpTemplatesToolset`
3. `UeremcpGameplay.UeremcpGameplayToolset`
4. `UeremcpAnimation.UeremcpAnimationToolset`
5. `UeremcpMaterial.UeremcpMaterialToolset`
6. `UeremcpNiagara.UeremcpNiagaraToolset`
7. `UeremcpBlueprint.UeremcpBlueprintToolset`

[VERIFIED-RUNTIME: `user-unreal-mcp.list_toolsets` against RE after rebuild of
`0099cb3`]

### At advanced main tip

Read-only `list_toolsets` after the advanced-main rebuild returned the same seven
plus:

8. `UeremcpValidation.UeremcpVisualCaptureToolset`

[VERIFIED-RUNTIME: `user-unreal-mcp.list_toolsets` against RE after rebuild of the
advanced local `main` tip; MCP listened on `127.0.0.1:8000`]

No Niagara domain tools were invoked. Unreal was stopped after verification, and
no listener remained on port 8000.
[VERIFIED-RUNTIME: process and TCP listener checks after stopping the editor]

## Final state

- Local `main` tip recorded by this update:
  `90fe0bf80e59fe8ff7019ee1329c33588466400a`
  (ancestor chain includes `0099cb3` and `a2bd21b`).
- RE junction target:
  `$UEREMCP_DEPLOY/Plugins/UEREMCP`.
- Deploy worktree checkout matches local `main`.
- Dedicated deploy worktree remains the default RE deployment path.
- No push was performed.

## Limitations

- Concurrent agents reused `UEREMCP-deploy-main` after the first alignment; the
  final tip therefore includes merges and docs landed by those agents, not only
  the audit commit.
- The first live verification at `0099cb3` is still valid evidence for the
  audit-only tip. The advanced tip was separately rebuilt and live-verified.
- Binary provenance for the final tip is the successful rebuild against the
  advanced plugin source, followed by docs-only tip commits that did not change
  plugin sources.
