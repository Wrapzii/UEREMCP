# Capability reference

**Owner:** WS-13. Statuses are from
[`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) on the full-use
integration tip. **Ready for practical use within cataloged partial scopes; not
production-perfect.** Pixel-delta PASS does not equal correct appearance or a
full metrics close.

**Routing:** [`tool-selection-policy.md`](tool-selection-policy.md). Prefer these
UEREMCP tools over Epic primitives for the same goal. Do not paste full graph
payloads into chat — load fixtures or [`examples/`](examples/).

## How to invoke

```
call_tool(toolset=<MCP toolset name>, tool=<PascalCase method>, arguments={ requestJson: "<envelope>" })
```

Exact toolset registration names come from `list_toolsets` at runtime (full names
like `UeremcpBlueprint.UeremcpBlueprintToolset`). Headers below document the C++
toolset class and method
`[VERIFIED: Plugins/UEREMCP/Source/Ueremcp*/Public/*Toolset.h]`.

Published examples (valid minimal + complete shapes):

- [`examples/minimal/`](examples/minimal/)
- [`examples/complete/`](examples/complete/)

Pass `tool_name` **without** the toolset prefix (`ReadGraph`, not
`…Toolset.ReadGraph`). Envelope argument key is camelCase `requestJson`.

These are **request-shape examples**, not proof that their sample target assets
exist in the currently open project. Before a read/inspect/capture, use the
policy-approved Epic `AssetTools.find_assets` read-only discovery call under the
allowed scratch roots, then replace `target.asset_path`. A missing sample target
must return an honest `rejected` response; it is not a reason to switch to
primitive mutation tools. See [`examples/README.md`](examples/README.md).

---

## Blueprint — `read_graph` / `submit_graph`

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | MCP `UeremcpBlueprint.UeremcpBlueprintToolset` → `ReadGraph`, `SubmitGraph` |
| Spec schemas | [`read_graph.schema.json`](../../schemas/domains/blueprints/read_graph.schema.json), [`submit_graph.schema.json`](../../schemas/domains/blueprints/submit_graph.schema.json) |

### Read (minimal envelope)

Do **not** assume a fixed scratch Blueprint path exists in RE. Discover with
`AssetTools.find_assets` under `/Game/__UeremcpTests/` / `/Game/__UeremcpPoc/`, or
submit/create under those roots first. The guide example path below is illustrative.

```json
{
  "protocol_version": "1.0",
  "request_id": "bp-read-1",
  "action": "read_graph",
  "target": {
    "asset_path": "/Game/__UeremcpTests/<discovered_or_created_blueprint>"
  },
  "specification": {
    "graph_id": "EventGraph"
  },
  "options": {
    "response_detail": "complete"
  }
}
```

Offline `specification` gate fixture:
[`Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/fixtures/read_graph_spec.fixture.json`](../../Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/fixtures/read_graph_spec.fixture.json).

### Submit (`mode: replace`)

Put the graph JSON (ADR-0004 shape) in `specification` / per domain schema — see
representative graph fixture (lossy areas included):
[`submit_graph_replace.fixture.json`](../../Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/fixtures/submit_graph_replace.fixture.json).

Envelope sketch:

```json
{
  "protocol_version": "1.0",
  "request_id": "bp-submit-1",
  "action": "submit_graph",
  "mode": "replace",
  "target": {
    "asset_path": "/Game/__UeremcpTests/<discovered_or_created_blueprint>",
    "graph_id": "EventGraph"
  },
  "expected_revision": "<revision from prior read>",
  "idempotency_key": "bp-submit-1-attempt",
  "specification": { "$comment": "load submit_graph_replace.fixture.json shape" },
  "options": {
    "validate": true,
    "compile": true,
    "save": true,
    "response_detail": "complete"
  }
}
```

**Ceiling:** scoped CRT A1–A11 PASS on recorded tips for simple graphs; **not**
arbitrary complex graphs. `mode: patch` is **planned** (unimplemented). See
[`limitations.md`](limitations.md).

---

## Niagara — `inspect_system` / `create_niagara_effect`

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | `UUeremcpNiagaraToolset` → `InspectSystem`, `CreateNiagaraEffect` |
| Spec schemas | [`inspect_system.schema.json`](../../schemas/domains/niagara/inspect_system.schema.json), [`create_niagara_effect.schema.json`](../../schemas/domains/niagara/create_niagara_effect.schema.json) |

### Inspect

The sample path is illustrative. Discover an existing scratch Niagara system
first; `InspectSystem` is fail-soft and rejects a missing/wrong-type target
without crashing the editor.

```json
{
  "protocol_version": "1.0",
  "request_id": "ns-inspect-1",
  "action": "inspect_system",
  "target": {
    "asset_path": "/Game/__UeremcpTests/Niagara/NS_InspectProbe"
  },
  "options": { "response_detail": "complete" }
}
```

Probe fixture:
[`schemas/domains/niagara/fixtures/inspect_probe_minimal.json`](../../schemas/domains/niagara/fixtures/inspect_probe_minimal.json).

### Create (POC-B shaped)

Canonical single-call request (field `request` inside the handoff JSON):
[`schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json`](../../schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json).

Key `specification` fields: `name`, `effect_type`, `element`, `components[]`,
`parameters`, `template_system`, `materials` (reuse / create_spec). Target roots
allowed by the fixture handoff: `/Game/__UeremcpPoc/`, `/Game/__UeremcpTests/`.

B10 rendered warm-pixel / particle gate is **PASS** via
`UEREMCP.Niagara.POCB.VisibleRender`. Structural create can still return
`partially_completed` when a domain check is skipped — read `capability_notes` /
`checks_skipped`. Do not equate B10 PASS with production visual perfection or E7
metrics close. Materials contract companion:
[`poc_b_fireball_materials.json`](../../schemas/domains/niagara/fixtures/poc_b_fireball_materials.json).

---

## Visual verification — `capture_effect_frames`

| | |
|---|---|
| Status | **available** (warm path + fresh-editor cold poll verified; appearance judgment still out of scope) |
| Toolset / tool | `UeremcpValidation.UeremcpVisualCaptureToolset` → `CaptureEffectFrames` |
| Spec schema | [`capture_effect_frames.schema.json`](../../schemas/domains/validation/capture_effect_frames.schema.json) |

```json
{
  "protocol_version": "1.0",
  "request_id": "vfx-capture-1",
  "action": "capture_effect_frames",
  "target": {
    "asset_path": "/Game/RE/VFX/Magecraft/Spells/NS_Spell_IceWall_Cast"
  },
  "specification": {
    "frame_count": 4,
    "duration_seconds": 1.5,
    "camera": "three_quarter",
    "width": 640,
    "height": 360
  },
  "options": { "validate": true }
}
```

Minimal dry-run: [`examples/minimal/capture_effect_frames.json`](examples/minimal/capture_effect_frames.json).
Complete live-shaped: [`examples/complete/capture_effect_frames.json`](examples/complete/capture_effect_frames.json).

The response returns PNG paths under
`Saved/UEREMCP/VfxCapture/<asset>/<request-id>/`, per-frame luminance data, and
`verification.rendered_something`. A cold renderer may first return
`partially_completed`; poll its non-cancellable `get_job_result` handle. This
fresh-editor path completed in two MCP round trips with 44 changed lit pixels
`[VERIFIED-RUNTIME: RE, full-use-cold-delay-20260730, 2026-07-30]`.

**Ceiling:** requires an editor world and renderer/RHI. Pixel delta proves only
that the subject changed the captured frame; it does not judge appearance,
compile validity, material correctness, or gameplay integration. Prefer this for
pixel evidence after structural create/inspect — not instead of them.

Also on the same toolset (status **partial** until live checklist):
`CaptureWorldFrames` (`capture_world_frames`), `CaptureMaterialFrames`
(`capture_material_frames`), `CaptureAnimationFrames`
(`capture_animation_frames`). See
[`tests/visual/MATERIAL_CAPTURE_PROTOCOL.md`](../../tests/visual/MATERIAL_CAPTURE_PROTOCOL.md)
and
[`tests/visual/ANIMATION_CAPTURE_PROTOCOL.md`](../../tests/visual/ANIMATION_CAPTURE_PROTOCOL.md).

---

## Environment — `build_environment` / `inspect_environment` / `validate_environment`

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | `UeremcpEnvironment.UeremcpEnvironmentToolset` → `BuildEnvironment`, `InspectEnvironment`, `ValidateEnvironment` (+ stage primitives) |
| Spec schema | [`build_environment.schema.json`](../../schemas/domains/environment/build_environment.schema.json) |

```json
{
  "protocol_version": "1.0",
  "action": "build_environment",
  "target": { "asset_path": "/Game/__UeremcpPoc/MountainRiverRain" },
  "options": { "dry_run": true, "validate": true },
  "specification": { "seed": 42 }
}
```

Prefer `BuildEnvironment` for mountain/river/forest/rain. Screenshots never gate
acceptance — pair with `ValidateEnvironment` then optional `CaptureWorldFrames`.

---

## Systems — audio / replication / world partition

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | `UeremcpSystems.UeremcpSystemsToolset` → `CreateAudioCue`, `InspectAudio`, `ValidateReplication`, `InspectWorldPartition`, `RepairWorldPartition` |
| Spec schemas | [`schemas/domains/audio/`](../../schemas/domains/audio/), [`networking/`](../../schemas/domains/networking/), [`world_partition/`](../../schemas/domains/world_partition/) |

```json
{
  "protocol_version": "1.0",
  "action": "create_audio_cue",
  "target": { "asset_path": "/Game/__UeremcpTests/Audio/SC_Cast" },
  "options": { "dry_run": true },
  "specification": {
    "sound_waves": ["/Game/Audio/SW_Cast"],
    "create_attenuation": true
  }
}
```

`RepairWorldPartition` defaults to `options.dry_run: true` and must no-op without
explicit `dry_run: false` plus destructive opt-in. MetaSound graph authoring and
WP HLOD builders remain blocked.

---

## Material — `create_vfx_material`

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | `UUeremcpMaterialToolset` → `CreateVfxMaterial` (+ `CreateProceduralTexture`) |
| Spec schema | [`create_vfx_material.schema.json`](../../schemas/domains/materials/create_vfx_material.schema.json) |

Honesty / validate=false probe (expects `partially_completed`):
[`schemas/domains/materials/fixtures/create_vfx_material_honesty_probe.json`](../../schemas/domains/materials/fixtures/create_vfx_material_honesty_probe.json).

Minimal live-shaped request from that fixture's `request` object — fields:
`purpose` (`elemental_projectile_core` / trail aliases), `element`, `features[]`,
`target.asset_path` under `/Game/__UeremcpTests/`.

Also: dry plan fixture
[`execute_plan_create_vfx_material_dry.json`](../../schemas/domains/materials/fixtures/execute_plan_create_vfx_material_dry.json)
(for internal plan / test use — see execute_plan note below).

---

## Templates — `search_templates` / `instantiate_template` / `promote_to_template`

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | `UUeremcpTemplatesToolset` → `SearchTemplates`, `InstantiateTemplate`, `PromoteToTemplate` |
| Spec schemas | [`schemas/domains/templates/`](../../schemas/domains/templates/) |

### Search

```json
{
  "protocol_version": "1.0",
  "request_id": "tpl-search-1",
  "action": "search_templates",
  "specification": {
    "query": "projectile",
    "domain": "niagara",
    "element": "fire",
    "limit": 20
  }
}
```

### Instantiate

```json
{
  "protocol_version": "1.0",
  "request_id": "tpl-inst-1",
  "action": "instantiate_template",
  "target": {
    "asset_path": "/Game/__UeremcpTests/Templates/NS_FromTemplate"
  },
  "specification": {
    "template_id": "niagara.projectile.elemental.v1",
    "inputs": { "element": "fire" },
    "modifiers": { "preserve": ["preserve_networking"] }
  },
  "options": { "validate": true }
}
```

Template documents live under [`templates/`](../../templates/) (see
[`template-authoring.md`](template-authoring.md)). Instantiation materializes an
internal `execute_plan` and delegates to the WS-05 interpreter
`[VERIFIED: UeremcpTemplatesToolset.cpp / UeremcpPlanExecutor]`.

### Promote

Preview-oriented until cross-domain security / schema gates bind. Spec:
[`promote_to_template.schema.json`](../../schemas/domains/templates/promote_to_template.schema.json)
— required `source_asset`; default `quarantine: true`.

---

## `execute_plan` — agent-facing on Reference

| Catalog status | **partial** — durable Claim/Complete + honest caveats |
|---|---|
| AICallable? | **Yes** — `UUeremcpReferenceToolset::ExecutePlan` `[VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpReferenceToolset.h]` |

Prefer goal tools / `InstantiateTemplate` when a template fits. `ExecutePlan` is the
ADR-0008 path for multi-step plans. Offline plan-shaped fixtures exist for harnesses,
e.g.
[`schemas/domains/niagara/fixtures/execute_plan_create_niagara_effect_dry.json`](../../schemas/domains/niagara/fixtures/execute_plan_create_niagara_effect_dry.json).

Durable idempotency caveats (metadata/package non-atomicity, stale claim reclaim,
legacy `Put`/`TryGetReplay` sites): [`limitations.md`](limitations.md).

---

## Jobs — `get_job_result` / `cancel_job`

| | |
|---|---|
| Status | **available** — process-local polling and cooperative cancellation |
| Toolset / tools | `UUeremcpReferenceToolset` → `GetJobResult`, `CancelJob` |

```json
{
  "protocol_version": "1.0",
  "action": "get_job_result",
  "specification": { "job_id": "<from partially_completed.job>" }
}
```

For a job that advertises `cancellable: true`, send the same envelope with
`"action": "cancel_job"` and its `job_id`. Cancellation is cooperative and
editor-verified; poll the retained job envelope until terminal
`job.state: cancelled`. A cancelled job must not claim validated completion.

Do not substitute Epic MCP `notifications/cancelled`: UE 5.8's private
ToolsetRegistry adapter has no `CancelAsync` override, so that notification cannot
reach AICallable work. This is an immutable adapter limitation, not an open
`cancel_job` residual — [`limitations.md`](limitations.md).

Minimal fixtures: [`examples/minimal/get_job_result.json`](examples/minimal/get_job_result.json),
[`examples/minimal/cancel_job.json`](examples/minimal/cancel_job.json).

---

## Reference — `ping` / `echo`

| Status | **available** |
|---|---|
| Tools | `Ping`, `Echo` on Reference (+ domain Echo/Ping where registered) |

Use these to separate protocol/registration failures from domain failures.

---

## Visual capture — `capture_effect_frames`

| | |
|---|---|
| Status | **partial** |
| Toolset / tools | MCP `UeremcpValidation.UeremcpVisualCaptureToolset` → `CaptureEffectFrames` |
| Spec schema | [`capture-effect-frames.schema.json`](../../schemas/domains/validation/capture-effect-frames.schema.json) |

```json
{
  "protocol_version": "1.0",
  "request_id": "visual-capture-1",
  "action": "capture_effect_frames",
  "target": { "asset_path": "/Game/__UeremcpPoc/FreshAgent/NS_FreshAgent_Probe" },
  "specification": {
    "frame_count": 4,
    "duration_seconds": 1.0,
    "camera": "three_quarter",
    "width": 640,
    "height": 360
  },
  "options": { "validate": true, "dry_run": false }
}
```

- Output directory: `<Project>/Saved/UEREMCP/VfxCapture/<asset>/<request-id>/`
  (`baseline.png`, `age_XX.png`, …). Response carries paths + luminance/pixel
  deltas — **not** image bytes. Open the PNGs to display them.
- Requires editor renderer (not NullRHI). `options.validate` must be `true`.
- Cold vs warm: cold may return `partially_completed` + ADR-0009 job
  (`cancellable: false`); poll `get_job_result` once. A second zero-pixel
  result remains `failed_validation`.
- Read-only on the Niagara asset; does not compile or author it.
- Catalog / guide: see [`agent-usage.md`](agent-usage.md) §9 and
  [`limitations.md`](limitations.md).

---

## Explicitly out of agent critical path on this tip

| Action | Why |
|---|---|
| Blueprint `patch` / `analyze_blueprint` / `create_blueprint` | planned |
| `create_spell` | partial — live plan upsert under scratch; POC D MET; D5 live multi-client proof |
| Animation inspect / AnimBP read | partial read-only; authoring unsupported |
| `list_domains` / `describe_action` | planned |

When in doubt, trust the catalog over this page and open a WS-13 proposal if the
guide drifts.
