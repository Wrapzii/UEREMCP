# Troubleshooting

**Owner:** WS-13. Pair with [`agent-usage.md`](agent-usage.md) and
[`limitations.md`](limitations.md).

## Status vocabulary (read this first)

Honest statuses only (`AGENTS.md` rule 6). Common ones:

| Status | Meaning |
|---|---|
| `created_and_validated` / `modified_and_validated` | Work done **and** re-read / verified |
| `created_with_warnings` | Created; warnings remain — read notes |
| `no_change_required` | Idempotent / already matches intent |
| `failed_validation` | Ran; verification failed |
| `rolled_back` | Failure discarded via sandbox / transaction |
| `partially_completed` | Incomplete, timed out, skipped check, or job still running |
| `rejected` | Refused before mutation (schema, revision, permission, …) |
| `error` | Unexpected failure |

**Never** treat "the tool returned" as success. Re-read `status`, `summary`,
`capability_notes`, and any `validation` / `diagnostics` blocks.

If a response claims `*_validated` without verification evidence in the payload,
treat the response as defective and report it — do not proceed as success.

## Diagnostics and graphs

At `response_detail: diagnostic` or `complete`, expect graph payloads and
`diagnostics` (dead nodes, disconnected subgraphs, domain codes). Use them before
issuing a repair call — that is the cost-model win (`docs/WHY.md`).

## Common failure patterns

| Symptom | Likely cause | Next tool / step |
|---|---|---|
| `rejected` + revision text | Stale `expected_revision` | `ReadGraph` / `InspectSystem` again → resubmit with new revision |
| `rejected` + dry_run / destructive | Security forced dry-run or missing `allow_destructive` | See [`SECURITY.md`](../SECURITY.md); explicit `dry_run: false` only when intentional |
| `partially_completed` on Niagara create | Domain skipped a check, timed out, or queued | Read `capability_notes` / `checks_skipped`; optional `CaptureEffectFrames` for pixel evidence — do not invent appearance PASS |
| `partially_completed` + `job` | Timeout / cold-capture path (ADR-0009) | `GetJobResult` (`action=get_job_result`); cancel via `CancelJob` if `cancellable: true` |
| MCP cancel returned HTTP 202 but work continues | UE 5.8's private ToolsetRegistry adapter has no `CancelAsync`; 202 proves notification acceptance only | Use UEREMCP `cancel_job(job_id)`, then poll `get_job_result`; see [`limitations.md`](limitations.md) |
| Tool not in `list_toolsets` | Plugin module not loaded / Live Coding | Human: rebuild/enable UEREMCP; agent: `Ping` on Reference |
| `ExecutePlan` missing / wrong altitude | Prefer templates or domain tools for most goals | `InstantiateTemplate` or domain goal tools — see [`tool-selection-policy.md`](tool-selection-policy.md); `ExecutePlan` is registered but not the first choice |
| Used Epic `create_node` / Niagara primitives and got stuck | Wrong altitude for the goal | Switch to UEREMCP semantic tool from the routing table |
| `CaptureEffectFrames` cold zero pixels | Renderer needs an editor-tick warmup | Poll the returned non-cancellable job; a second zero-pixel result is honest `failed_validation` for non-rendering systems |

## Metrics fields (do not over-interpret)

Response `metrics` may include `mcp_round_trips`, `internal_operations`,
`replayed`, timing. POC-B token / wall metrics are **partial** —
[`docs/reviews/poc-metrics.md`](../reviews/poc-metrics.md). Missing tokens ≠ success.
