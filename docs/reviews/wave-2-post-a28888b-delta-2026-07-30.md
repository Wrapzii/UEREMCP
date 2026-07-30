# Wave 2 critic delta: post-`a28888b`

- **Reviewer:** WS-14
- **Range:** findings after `3a621d1`; orch changes from `a28888b` through
  `01d121e`, plus WS-08 fix review at `23e3bda`
- **Method:** source/doc diff plus schema/unit checks below. No RE junction retarget,
  editor rebuild, or runtime tool discovery was performed.

## Verdict

The original registration Critical is now fixed in source. Material, Niagara, and
Templates each register their toolset on `OnPostEngineInit`, verify the registry state,
and unregister on shutdown; Animation follows the same pattern
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialModule.cpp:15-55]`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraModule.cpp:15-55]`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpTemplates/Private/UeremcpTemplatesModule.cpp:27-101]`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpAnimation/Private/UeremcpAnimationModule.cpp:13-44]`.
C-1 is therefore **closed as a source defect**. Agent visibility remains unverified
until the current integrated plugin builds and `list_toolsets` is run.

The post-baseline implementation is materially deeper: Niagara create/inspect now
keeps an honest `partially_completed` ceiling, Material has real editor mutations,
Blueprint has a read bridge, Animation is loaded/registered while withholding
non-graph state, and the asset-state amendment remains Proposed rather than silently
forking the frozen envelope.

## Prior findings

| Prior ID | Delta status |
|---|---|
| C-1 | **Closed in source.** Registration landed for Niagara (`66a15ad`), Material (`9e1b152`), and Templates (`45fd0ef`). |
| H-1 | **Closed.** Orch now commits `tests/integration/_logs/editor_UEREMCP_Validation_20260730_6of6.redacted.md`, with 6 PASS / 0 FAIL and explicit residual scope. |
| H-2 | **Open, narrowed.** The new redaction says “in-process ... fixtures,” but ADR-0006 still presents the generic domain acceptance test immediately above an unqualified runtime PASS. No domain equality/revision pipeline is proven. |
| H-3 | **Open, narrowed.** Timeout now has real response-contract assertions (`cb83b67`) and the JobRegistry implementation landed (`29a3da6`), but Transport poll and cancel remain SKIP stubs whose message still says the registry has not landed. No new committed Transport run artifact proves the changed 6 PASS / 2 SKIP source state. |
| H-4 | **Open, narrowed.** Gameplay now calls the shared path policy, but no domain calls `FUeremcpPermissionPolicy::Evaluate`; mutator queue and audit log remain explicit stubs. Material/Niagara mutation therefore still bypasses the ADR-0010 enforcement gate. |
| L-1 | **Closed.** `create_niagara_effect` now exists and remains below a validated status. |

## Resolved High finding

### H-5. Material operations can report `*_validated` when validation is disabled — closed

The envelope contract says `options.validate:false` forfeits every `*_validated`
status and requires `partially_completed`
`[VERIFIED: schemas/envelope/request.schema.json:79-82]`.

`create_vfx_material` conditionally skips its re-read when `Request.bValidate` is
false, then unconditionally assigns `created_and_validated` or
`modified_and_validated` and unconditionally says “re-read verified”
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp:409-448]`.
It also leaves an update at `modified_and_validated` when requested features are
unimplemented; only the create branch is downgraded to `created_with_warnings`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp:438-445]`.

The new procedural-texture path does not propagate `Request.bValidate` at all and
returns `created_and_validated` after an in-memory dimension re-read, including when
the caller disabled validation
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp:247-272,291-323]`.

**Impact:** a valid envelope option can produce false verification, violating the
response contract and the repository's success rule.

**Required action:** force `partially_completed` when validation is disabled; make
summary text conditional on checks actually run; downgrade both create and modify when
features are skipped; add `validate:false` contract tests for both Material actions.

**Resolution (`23e3bda`, reviewed from `ws-08-material`):**

- `ResolveMaterialSuccessStatus` returns `partially_completed` before any
  `*_validated` branch when `bValidate` is false
  `[VERIFIED: $UEREMCP_ROOT-ws08/Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp:324-337]`.
- An existing instance with unimplemented features now resolves to
  `partially_completed`; a newly created instance resolves to
  `created_with_warnings`, never `modified_and_validated`
  `[VERIFIED: $UEREMCP_ROOT-ws08/Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp:333-337]`.
- Procedural texture requests now carry `Request.bValidate`; `false` returns
  `partially_completed` before the dimension re-read and validated status
  `[VERIFIED: $UEREMCP_ROOT-ws08/Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp:247-270]`
  `[VERIFIED: $UEREMCP_ROOT-ws08/Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp:343-348]`.
- Both summaries are conditional and no longer claim checks that were skipped.
  Offline H-5 contracts pass 6/6, including all three cases above; the two new editor
  tests are source-present but were not run by WS-14.

**Status:** **Closed in source and offline contract tests.** Runtime Material
automation remains a broader integration gate, not an H-5 logic blocker.

## New Medium overclaim

Commit `f3a8043` is titled “Verify InspectSystem automation on RE,” but its only change
documents that both target automation tests were **not run** because no binary was
produced
`[VERIFIED: docs/research/RB-07-niagara.md:490-510]`.
The document itself is honest; the commit subject is not and should not be cited as
runtime verification.

## Updated open counts

- **Critical: 0** (C-1 closed in source; runtime discovery still pending)
- **High: 3** (H-2, H-3, H-4)

Mitigated/closed in this delta: C-1, H-1, H-5, L-1; H-2, H-3, and H-4 narrowed.
