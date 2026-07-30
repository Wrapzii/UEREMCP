# Niagara and Material runtime handoffs

**Owner:** WS-11. **Status:** prepared; no runtime pass claimed here.

These profiles let the editor-owning lane run domain automation without changing the
RE plugin junction. The runner uses the project and junction already configured by the
operator.

## Shared preflight

1. Confirm the RE junction already targets the intended orchestration worktree. Do not
   retarget it for this handoff.
2. Confirm no other lane owns the editor. If it is contended, stop; these tests are not
   a reason to steal it.
3. Close interactive Unreal Editor / Live Coding before rebuilding stale modules.
4. Run one filter at a time. Keep raw logs local; commit only redacted result notes.
5. Treat process exit zero plus per-test `Success` lines as evidence. A tool response
   alone is not verification.

## Niagara inspection

Source fixture:
`Plugins/UEREMCP/Source/UeremcpNiagara/Private/Tests/UeremcpNiagaraInspectTests.cpp`.

```powershell
pwsh tests/run_editor_tests.ps1 `
  -KeepUeremcp -NoProbe `
  -Filter "UEREMCP.Niagara.Inspect"
```

Expected tests, not current results:

- `UEREMCP.Niagara.Inspect.PathGuard` checks that the probe path is restricted to
  `/Game/__UeremcpTests/`.
- `UEREMCP.Niagara.Inspect.NS_WS07_Probe` requires the fixture asset
  `/Game/__UeremcpTests/NS_WS07_Probe`; it checks parseable output, system and module
  stack graph entries, and positive internal-operation accounting.

Acceptance:

- Both tests report `Success` in the same run.
- The runtime fixture test must not be reported as passed if the probe asset was absent,
  the module failed to load, or Automation never started.
- This filter does not prove complete Niagara round-trip or construction. Its source
  currently expects `partially_completed`; record that exact scope.

## Material toolset

Source fixture:
`Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialToolsetTests.cpp`.

```powershell
pwsh tests/run_editor_tests.ps1 `
  -KeepUeremcp -NoProbe `
  -Filter "UeremcpMaterial.Toolset"
```

Expected tests, not current results:

- `UeremcpMaterial.Toolset.Echo`
- `UeremcpMaterial.Toolset.Register`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore`
- `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail`

The mutation fixtures use only `/Game/__UeremcpTests/Materials/` and attempt cleanup
before and after execution. Run with the existing junction; do not redirect RE to an
individual WS-08 worktree.

Acceptance:

- All four tests report `Success` in the same run.
- Core and Trail each return `created_and_validated` and the target material instance
  exists before cleanup.
- After the run, verify no `MI_WS08_*` or test master assets remain under the scratch
  root. If cleanup cannot be confirmed, report `created_with_warnings`, not a clean pass.
- This filter does not prove feature-driven graph wiring, procedural textures, Material
  Function composition, or purposes outside the two projectile fixtures.

## Redacted evidence template

Record branch/commit, existing junction target, exact command, process exit, each test
result, scratch cleanup result, and limitations. Use `[VERIFIED-RUNTIME: <local log>]`
only after inspecting the raw log. Until then, status remains **not run**.
