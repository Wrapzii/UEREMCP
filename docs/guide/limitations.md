# Limitations

**Owner:** WS-13. Aggregated from catalog, SECURITY, reviews, and tip
`ws-11-poc-b10-render` (`dae0e5c`). An undocumented limitation is a defect
(`AGENTS.md` rule 6).

**Do not claim overall POC-B.** Structural B1–B9 progress does not equal B10 or E7.

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
| B10 production failure | Visible warm-pixel / render signature **FAIL** on production fireball; create returns `partially_completed` until B10 + metrics close ([catalog](../CAPABILITY_CATALOG.md), [`poc-metrics.md`](../reviews/poc-metrics.md)) |
| Topology inspect | Intentionally lossy (event handler stacks, etc.) |
| POC C | `create_niagara_template` / `create_effect_variation` still **research** |

## POC-B metrics (WS-14)

[`docs/reviews/poc-metrics.md`](../reviews/poc-metrics.md) is **partial**:

- Measured: some `mcp_round_trips`, server-side interval lower bound
- **Unavailable / open:** client wall clock, full token totals, measured primitive baseline, overall POC-B completion
- E7 not satisfied for a full metrics close

## Security adoption (ADR-0010)

From [`docs/SECURITY.md`](../SECURITY.md) domain adoption table (2026-07-30):

| Domain | `FUeremcpMutatingDispatch` wired? |
|---|---|
| Blueprint `SubmitGraph` / `ReadGraph` | **yes** |
| Gameplay `CreateSpell` | **yes** (still preflight-only mutation semantics) |
| Niagara `CreateNiagaraEffect` | **no** — handoff proposal exists |
| Material `CreateVfxMaterial` / `CreateProceduralTexture` | **no** — handoff proposal exists |

**R-07 stays open** until Niagara and Material call the shared dispatch on live mutate
paths. Agents must not assume those domains enforce the same queue / audit / path gate
as Blueprint.

## Transport cancel adapter

ADR-0009 + RB-04: Epic MCP `notifications/cancelled` calls
`IModelContextProtocolTool::CancelAsync`, but ToolsetRegistry adapter does **not**
override CancelAsync — cancel is not wired through to registry tools
`[VERIFIED: UeremcpJobConstraints.cpp comments; ADR-0009; transport automation SKIP residual]`.

Use UEREMCP `cancel_job` for cooperative cancel when the domain honors it. Do not
advertise MCP notification cancel as proven.

`get_job_result` / `cancel_job` are registered but catalog-marked **partial** (timeout /
cancel SKIP residuals tracked in transport tests).

## `execute_plan`

Internal interpreter exists; **not** a direct `AICallable` on this tip. Agents that
call a fictional `ExecutePlan` tool will fail discovery.

## Gameplay / Animation / discovery

- `create_spell`: AICallable preflight only — **no** asset mutation; POC D not started
- Animation: inspect / AnimBP read partial; Control Rig may prove read-only
- UEREMCP `list_domains` / `describe_action`: planned — use Epic `list_toolsets`

## Root README staleness

Repository root [`README.md`](../../README.md) still says Phase 0 / "implementation has
not started." That is **stale relative to this tip** (owned by WS-01 — WS-13 does not
edit it). Prefer [`CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) and these guides
for capability truth.
