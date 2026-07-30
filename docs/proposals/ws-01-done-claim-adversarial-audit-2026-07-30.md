# Adversarial audit: “UEREMCP plugin is done”

- **Owner:** WS-01
- **Date:** 2026-07-30
- **Audit base:** local `main` at `baa0d0663b8488cce6ec76746ae65ffad5fd79eb`
- **Disposition:** POC A–E is documented as claimed; “done” without the POC qualifier is not supported.

## Verdict

The strongest supportable statement is:

> All 11 declared editor modules have build artifacts; seven UEREMCP toolsets were
> registered in recorded editor runs; and the repository documents POC A–E as
> claimed with scoped runtime evidence. The plugin is not production-ready, current
> `main` is not the source tree deployed through the RE junction, and current binary
> provenance is not tied to a source SHA.

This audit does **not** certify current live behavior. At audit time there was no
`UnrealEditor` process and no listener on `127.0.0.1:8000`; read-only
`list_toolsets` failed with connection refused.

## Claim-by-claim findings

### 1. “All 11 modules compiled; seven UEREMCP toolsets were live”

**Partly supported, with deployment caveats.**

`Plugins/UEREMCP/UEREMCP.uplugin` declares 11 editor modules at lines 14–80:
Protocol, Blueprint, Niagara, Material, Animation, Gameplay, Security, Templates,
Core, Transport, and Validation.

The deployed `UnrealEditor.modules` manifest names all 11 DLLs. The DLL timestamps
observed under the RE junction were:

- Animation `2026-07-30 08:02:42`
- Blueprint `06:41:30`
- Core `09:23:37`
- Gameplay `06:42:41`
- Material `09:47:57`
- Niagara `10:50:40`
- Protocol `09:27:57`
- Security `09:51:02`
- Templates `08:02:42`
- Transport `06:42:46`
- Validation `09:48:41`

This establishes **artifact presence**, not that the DLLs all came from one source
commit.

Recorded editor logs establish seven UEREMCP registrations:

1. `UeremcpCore.UeremcpReferenceToolset`
2. `UeremcpTemplates.UeremcpTemplatesToolset`
3. `UeremcpGameplay.UeremcpGameplayToolset`
4. `UeremcpAnimation.UeremcpAnimationToolset`
5. `UeremcpMaterial.UeremcpMaterialToolset`
6. `UeremcpNiagara.UeremcpNiagaraToolset`
7. `UeremcpBlueprint.UeremcpBlueprintToolset`

Exact evidence: `RE/Saved/Logs/RE.log:2339-2355`, and independently
`RE/Saved/Crashes/UECC-Windows-0CDB700346157DCCA196FD847146F248_0000/editor_UEREMCP_Validation_Gameplay_PatternB_MultiClientPIE_20260730_143636.log:2174-2190`.
Those logs prove registration in those runs, not current liveness.

Security is intentionally an application-layer library rather than a registered
toolset; Protocol, Transport, and Validation also do not add domain toolsets. Source
registration uses public `UToolsetRegistry::RegisterToolsetClass`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraModule.cpp:40-48]`.

### 2. “CAPABILITY_CATALOG says Phase 0”

**False for current `main`; true for the stale checked-out WS-11 worktree.**

At `main` SHA `baa0d06`, `docs/CAPABILITY_CATALOG.md:8-12` says POC A–E is
claimed and explicitly says **not production-ready**. Lines 49–52 say statuses
reflect registered code and runtime/editor evidence, not Phase 0 intent.
`README.md:10-16` gives the same POC-complete/not-production-ready boundary.

The Phase 0 sentence exists at
`$UEREMCP_ROOT/docs/CAPABILITY_CATALOG.md:42-43`
because that worktree is checked out at
`ws-11-editor-gate-runtime-followup` SHA
`091d8bf3b84eac1e1c31a3340c52f2be92880485`, 240 commits behind `main` with
local WS-11 edits. Its `README.md:10-12` is likewise stale.

No current-status correction is needed on `main`; historical/stale worktree docs
must not be rewritten as though they were current.

### 3. “32 branches are unmerged”

**Raw count is wrong and does not measure unintegrated value.**

There are 47 local branches including `main`:

- 15 branch tips are ancestors of `main`.
- 31 branch tips are not ancestors of `main`.
- Of those 31, 18 have no `+` entries from `git cherry main <branch>` and
  therefore have no unique patch IDs.
- 13 have at least one unique patch ID, but every one is an old divergent tree
  177–538 files different from current `main`; none is safe to merge wholesale.

Ancestor branches:

`ws-01-doc-certification`, `ws-01-hardening-integration`, `ws-01-orch`,
`ws-01-poc-cde-integration`, `ws-02-audit`,
`ws-04-transport-cancel-hardening`,
`ws-05-idempotency-persistence-hardening`, `ws-07-b10-warm-signature`,
`ws-07-integrated-particles`, `ws-07-niagara-status-honesty`,
`ws-11-b10-particle-count`, `ws-11-multiplayer-visual-hardening`,
`ws-11-poc-b10-render`, `ws-11-refresh-poc-b-evidence`,
`ws-12-13-cancel-doc-adoption`.

Non-ancestor branches with no unique patch IDs:

`ws-06-a6-proof`, `ws-07-create-runtime-evidence`,
`ws-07-niagara-baseline-fix`, `ws-07-niagara-runtime-spawn`,
`ws-07-niagara-status-honesty-2`, `ws-07-poc-c-ice-variation`,
`ws-07-registration-hotfix`, `ws-09-gameplay`,
`ws-09-poc-d-create-spell`, `ws-11-editor-handoffs`,
`ws-11-niagara-editor-gate`, `ws-11-poc-e-durability`,
`ws-11-validation`, `ws-11-validation-core-dep`,
`ws-11-validation-security-dep`, `ws-12-security`, `ws-13-agent-guides`,
`ws-15-templates`.

Branches with unique patch IDs, grouped by workstream:

- **WS-01:** `ws-01-acceptance-gap-audit-2026-07-30`
- **WS-03:** `ws-03-plugin`
- **WS-04:** `ws-04-transport`
- **WS-05:** `ws-05-protocol`
- **WS-06:** `ws-06-blueprint`
- **WS-07:** `ws-07-niagara`, `ws-07-registration-only`
- **WS-08:** `ws-08-material`
- **WS-10:** `ws-10-animation`
- **WS-11:** `ws-11-editor-gate-runtime-followup`
- **WS-14:** `ws-14-poc-b-metrics`, `ws-14-review`
- **WS-15:** `ws-15-registration-hotfix`

Content audit found zero branch-added paths absent from `main` for 12 of those 13.
Their work is merged by different commits or superseded by later implementations.
Examples on `main`: Blueprint patch rejection `6decd88`; timeout scheduler
`dae0e5c`; Templates registration `45fd0ef`; ExecutePlan exposure `fc98fbc`;
metrics harness `5499f48`; production B10 image lineage `87d6c81`.

The only branch-added paths absent from `main` are six old WS-06 offline
fixture/helper files:

- `Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/fixtures/request_read_graph.fixture.json`
- `.../request_submit_graph_dry_run.fixture.json`
- `.../response_submit_graph_dry_run.fixture.json`
- `Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/py/blueprint_fidelity.py`
- `.../test_blueprint_envelope_fixtures.py`
- `.../test_blueprint_fidelity.py`

These are a **review candidate, not established missing product work**. The old
branch deletes or rewinds newer main tests in the same tree. WS-06 should compare
the six files against current coverage and cherry-pick/rewrite only tests that add
new assertions.

### 4. “Epic GetSystemSummary may crash the editor”

**A historical crash in the shared path is confirmed; the direct Epic MCP-tool
claim remains unconfirmed on current code.**

Epic owns the MCP tool
`NiagaraToolsets.NiagaraToolset_System.GetSystemSummary`. Its live-captured schema
requires:

```json
{"system":{"refPath":"/Game/X/NS_Y.NS_Y"}}
```

`[VERIFIED: docs/audit/raw/schemas/NiagaraToolsets.NiagaraToolset_System.json:1875-1899]`.
The tool declaration is Epic code
`[VERIFIED: Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_System.h:207-218]`;
its wrapper delegates to `UNiagaraExternalEditUtilities::GetSystemSummary`
`[VERIFIED: .../NiagaraToolset_System.cpp:134-143]`. The utility is implemented
in NiagaraEditor and checks the system, reads exposed user parameters, then walks
emitter handles
`[VERIFIED: Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/NiagaraExternalSystemEditorUtilities.cpp:1409-1444]`.

UEREMCP does **not** route or re-export that Epic MCP tool by name. It registers a
separate `UeremcpNiagara.UeremcpNiagaraToolset`. However, its `inspect_system`
implementation calls the same underlying
`UNiagaraExternalEditUtilities::GetSystemSummary`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraInspect.cpp:402-415]`.
Therefore “merely coexists” is inaccurate: it independently wraps the same
NiagaraEditor API.

There is concrete historical crash evidence:

- Crash GUID `UECC-Windows-8598519243B4E73E9F886E82FBAA23E1_0000`
- UE `5.8.0-55116800`, `UnrealEditor-Cmd`, access violation reading null
- call stack reaches `FUeremcpNiagaraInspect::Run` at line 412, called by
  `ValidateCreateResult`, `CreateNiagaraEffect`, and the six-emitter acceptance test
- test scratch asset:
  `/Game/__UeremcpTests/NS_POCB_FireballProbe.NS_POCB_FireballProbe`
- exact evidence:
  `RE/Saved/Crashes/UECC-Windows-8598519243B4E73E9F886E82FBAA23E1_0000/CrashContext.runtime-xml:11,30,75-89`
  and sibling log `:2945-3000`

That evidence confirms a crash in UEREMCP's post-create inspect path involving
Niagara/NiagaraEditor. It does not by itself prove that the top-level Epic
`GetSystemSummary` MCP wrapper is the fault, nor that current `main` still crashes.
Later same-day POC-B runs passed after Niagara lifecycle/compile fixes, so the
current-state classification is **historical confirmed crash; current regression
status not established**.

The separate uncommitted WS-11 note claiming a valid direct Epic-tool dispatch
preceded termination has no matching dispatch line in retained logs and no
`GetSystemSummary` entry in `Saved/mcp_probe/mcp_call_log.jsonl`. Retained helper
scripts pass a plain string rather than the schema-required `{refPath}` object.
Do not automate the direct Epic call until a throwaway project or scratch asset,
watchdog, exact request capture, and crash dump correlation are in place.

## Deployment alignment

At audit end:

- RE junction:
  `$UEREMCP_LEGACY_PROJECT/Plugins/UEREMCP`
  → `$UEREMCP_ROOT-ws01/Plugins/UEREMCP`
- junction source worktree SHA:
  `b84397fa6ccbe92fe45fd2cdf7b9efd2b6f8aac7`
  (`ws-07-niagara-status-honesty`)
- current `main`:
  `baa0d0663b8488cce6ec76746ae65ffad5fd79eb`
- editor processes: zero
- port 8000 listeners: zero

The junction was not changed during this audit. Current `main` hardening/docs are
therefore **not deployed as source through the RE junction**. DLL timestamps prove
separate module builds occurred, but no embedded source SHA or reproducible build
manifest ties those DLLs to either `b84397f` or `baa0d06`. Live inference must not
claim current-main behavior from those binaries.

## Truthful completion levels

1. **Compiled / loads:** supported historically. Eleven DLLs exist and seven
   UEREMCP toolsets registered in recorded runs.
2. **POC A–E:** supported as a documented, scoped claim on `main`, not independently
   re-executed in this audit. `docs/CAPABILITY_CATALOG.md:49-123` preserves many
   `partial`, `planned`, and `research` statuses.
3. **Documented feature completeness:** not supported. Blueprint patching,
   generic Blueprint creation, validation actions, broad material creation,
   Animation authoring, Control Rig authoring, and later domains remain absent or
   research.
4. **Production readiness:** explicitly not supported
   (`README.md:10-16`; `docs/CAPABILITY_CATALOG.md:8-12,49-52`).
5. **Deployment alignment to current main:** not supported; junction SHA differs,
   editor is offline, and binary provenance is not commit-addressable.

## Required next actions

1. Add a build provenance artifact containing source SHA, dirty state, UE build ID,
   and DLL hashes; expose it through a read-only diagnostics tool.
2. Deploy one reviewed source SHA (preferably an ancestor of current `main`) to the
   RE junction, rebuild all 11 modules together, restart, and capture
   `list_toolsets` plus `describe_toolset` for the seven UEREMCP names.
3. Add a guarded regression for the confirmed Niagara post-create inspect crash
   using only `/Game/__UeremcpTests/`, with out-of-process crash monitoring.
4. Separately isolate Epic's direct `GetSystemSummary` in a throwaway project or
   known-good scratch system with exact schema-shaped parameters. Until then,
   mark the direct Epic-tool crash attribution as unresolved.
5. Have WS-06 review only the six branch-only offline fixture/helper files. Do not
   merge any stale workstream branch wholesale.
6. Use the phrase **“POC A–E claimed; not production-ready; deployed SHA
   unverified”** until deployment provenance and the Niagara regression are closed.
