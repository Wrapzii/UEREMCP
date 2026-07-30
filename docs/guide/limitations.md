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
| B10 production residual | Warm-pixel / render signature gaps may remain; structural create is live. See closeout. |
| Topology inspect | Intentionally lossy (event handler stacks, etc.) |
| POC C | Claimed under accepted criteria; variation + C7 third generation proven |

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
| Niagara `CreateNiagaraEffect` | **yes** — MutatingDispatch + Domain E3/E4 gates |
| Material `CreateVfxMaterial` / `CreateProceduralTexture` | **yes** — MutatingDispatch + Domain E3/E4 gates |

**R-07 closed** for Niagara and Material live mutators on the POC closeout tip.

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

Agent-facing `UUeremcpReferenceToolset::ExecutePlan` is registered; Templates also bind
the executor. Prefer live plan evidence in POC D claim docs.

## Gameplay / Animation / discovery

- `create_spell`: live upsert via plan under scratch paths; POC D MET (D5 static)
- Animation: inspect / AnimBP read partial; Control Rig may prove read-only
- UEREMCP `list_domains` / `describe_action`: planned — use Epic `list_toolsets`

## Root README

Repository root [`README.md`](../../README.md) reflects POC A–E claimed /
not-production-ready. Prefer [`CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) and
[`ws-01-poc-closeout-2026-07-30.md`](../proposals/ws-01-poc-closeout-2026-07-30.md)
for capability truth.
