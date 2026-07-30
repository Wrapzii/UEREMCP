# Limitations

**Owner:** WS-13. Aggregated from catalog, SECURITY, reviews, and the
post-hardening tip (parent `6a611cf` / documentation certification). An
undocumented limitation is a defect (`AGENTS.md` rule 6).

**POC A–E claimed; not production-ready.** Structural POC B is claimed with B10
warm-pixel PASS — that is **not** production visual perfection or a full metrics
close (E7 / R-17).

## Blueprint

| Limit | Detail |
|---|---|
| Complex graphs | Scoped CRT A1–A11 PASS on simple fixtures; **not** arbitrary complex Blueprint graphs ([catalog](../CAPABILITY_CATALOG.md)) |
| Patch mode | `modify_blueprint_graph` / `mode: patch` is **planned** — unimplemented A8 escape hatch |
| Lossy areas | MultiGate decompile-on-exec, Timeline spawn syntax, bind elision, reroute knots, GUID/position non-semantic, unproven custom K2 — see fixture `fidelity.lossy_areas` in [`submit_graph_replace.fixture.json`](../../Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/fixtures/submit_graph_replace.fixture.json) |
| Metrics | Recorded CRT runs may show `tokens_total=0` — do not invent token math |

## Niagara / POC-B

| Limit | Detail |
|---|---|
| B10 | Programmatic warm-pixel / particle gate **PASS** via `UEREMCP.Niagara.POCB.VisibleRender` (`tests/run_poc_b10_visible_render.ps1`). Does **not** claim correct look on every scene/hardware/quality setting |
| Topology inspect | Intentionally lossy (event handler stacks, etc.) |
| POC C | Claimed under accepted criteria; variation + C7 third generation proven |

## POC-B metrics (WS-14)

[`docs/reviews/poc-metrics.md`](../reviews/poc-metrics.md) is **partial**:

- Measured: some `mcp_round_trips`, server-side interval lower bound
- **Unavailable / open:** client wall clock, full token totals, measured primitive baseline, overall metrics close
- E7 not satisfied for a full metrics close — do not equate B10 PASS with E7

## Security adoption (ADR-0010)

From [`docs/SECURITY.md`](../SECURITY.md) domain adoption table (2026-07-30):

| Domain | `FUeremcpMutatingDispatch` wired? |
|---|---|
| Blueprint `SubmitGraph` / `ReadGraph` | **yes** |
| Gameplay `CreateSpell` | **yes** (still preflight-only mutation semantics) |
| Niagara `CreateNiagaraEffect` | **yes** — MutatingDispatch + Domain E3/E4 gates |
| Material `CreateVfxMaterial` / `CreateProceduralTexture` | **yes** — MutatingDispatch + Domain E3/E4 gates |

**R-07 mitigated** for those wired live mutators. **R-07 residual remains** for
mutate paths that skip the gate (Animation writes if added; Templates promote;
future domains). **R-12 mitigated** via `FUeremcpMutatorQueue` for gated writers.

## Cooperative cancellation and the Epic adapter limit

UEREMCP `cancel_job(job_id)` is available for jobs that advertise
`cancellable: true`. The production scheduler path is editor-verified: the worker
observed its cooperative token, ran its domain rollback checkpoint, stopped before
validated completion, and remained pollable as terminal `job.state: cancelled`
`[VERIFIED-RUNTIME: UEREMCP.Transport.JobRegistry.Cancel,
editor_UEREMCP_Transport_20260730_143347.log, 8/8 Success;
docs/proposals/ws-04-cancellation-hardening-closeout.md:51-68]`.

Separately, Epic MCP `notifications/cancelled` cannot reach
ToolsetRegistry/AICallable work in UE 5.8. Epic's private ToolsetRegistry adapter and
tool-search `FCallTool` do not override `CancelAsync`
`[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.h:13-26;
$UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolSearch.h:61-80]`.
This is an immutable adapter limitation, not an open UEREMCP `cancel_job` residual.
HTTP 202 for the MCP notification proves only notification acceptance. Operators and
agents must use UEREMCP `cancel_job(job_id)` and poll `get_job_result`.

## `execute_plan` and durable idempotency

Agent-facing `UUeremcpReferenceToolset::ExecutePlan` is registered (`AICallable`).
Durable Claim/Complete under `Saved/UEREMCP/idempotency` is verified for
`execute_plan` (restart Create/Verify pair). Honest caveats:

- metadata + package files are **not** one atomic transaction
- crash-after-mutation-before-completion leaves a reclaimable in-progress claim (~1h)
- legacy `Put` / `TryGetReplay` call sites lack fingerprint conflict detection until
  migrated (`execute_plan` is migrated)

## Gameplay / Animation / discovery

- `create_spell`: live upsert via plan under scratch paths; POC D MET; D5 static
  Pattern B minimum **plus** live multi-client listen-server proof
  (`tests/run_d5_multiclient.ps1`)
- Animation: inspect / AnimBP read partial; Control Rig may prove read-only
- UEREMCP `list_domains` / `describe_action`: planned — use Epic `list_toolsets`

## Root README

Repository root [`README.md`](../../README.md) reflects POC A–E claimed /
not-production-ready. Prefer [`CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) and
[`ws-01-hardening-consolidation-2026-07-30.md`](../proposals/ws-01-hardening-consolidation-2026-07-30.md)
for capability truth.
