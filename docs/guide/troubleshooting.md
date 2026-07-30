# Troubleshooting

**Owner:** WS-13. Status vocabulary is frozen in
[`schemas/common/defs.schema.json`](../../schemas/common/defs.schema.json) `$defs.status`.

## Status meanings

| Status | Means | Agent action |
|---|---|---|
| `created_and_validated` | Created **and** re-read / checks passed | Trust only if `validation` booleans match; inspect `checks_performed` |
| `modified_and_validated` | Modified **and** verified | Same — require `reread_after_write: true` when that was the contract (POC A7) |
| `created_with_warnings` | Created; warnings in `validation.warnings` / diagnostics | Read warnings; decide fix vs accept |
| `no_change_required` | Spec already matches reality | Do not recompile / rewrite; success for idempotent replace |
| `failed_validation` | Write or compile checks failed with actionable errors | Fix from `validation.errors` / `diagnostics`; do not retry blindly |
| `rolled_back` | Sandbox discarded after failure | Confirm assets absent; fix cause before retry |
| `partially_completed` | Could not fully verify, timed out, queued, validate=false, or known POC gap | Read `summary`, `job`, `capability_notes`, `checks_skipped` — **never** treat as `*_validated` |
| `rejected` | Policy, revision conflict, bad envelope, collision | Fix request; on revision conflict use returned revision |
| `error` | Unexpected failure | Capture `summary` + logs; do not invent success |

Honesty rules (`AGENTS.md` rule 6):

- `options.validate: false` → must be `partially_completed`, never `*_validated`
- Non-empty `result.unresolved_dependencies` is incompatible with `*_validated`
- Unperformed checks are `null` or listed in `checks_skipped` — never silently `true`

## How to consume the change manifest (`result`)

| Field | Use |
|---|---|
| `primary_asset` | Soft path of the main outcome |
| `created_assets` / `modified_assets` / `deleted_assets` | What changed — verify against intent |
| `reused_assets` | Existing assets depended on (vocabulary for next calls) |
| `dependencies` / `dependencies_created` | Closure the agent would otherwise re-query |
| `unresolved_dependencies` | Blockers — incompatible with validated success |
| `operations[]` | Per-op status for batch / plan results |

Prefer `response_detail: summary` or richer so the manifest is present in one call.

## How to consume `validation`

| Field | Use |
|---|---|
| `compiled` / `saved` / `structurally_valid` / `dependencies_resolved` | Evidence bits — must be real checks |
| `reread_after_write` | Did the service compare post-write state to intent? |
| `checks_performed` | Named checks that ran (e.g. `niagara.renderer_bound`) |
| `checks_skipped` | What did **not** run and why |
| `warnings` / `errors` | Diagnostic objects (`severity`, `code`, `message`, optional paths) |

If `status` says validated but `checks_performed` is empty and booleans are null,
treat the response as defective and report it — do not proceed as success.

## Diagnostics and graphs

At `response_detail: diagnostic` or `complete`, expect graph payloads and
`diagnostics` (dead nodes, disconnected subgraphs, domain codes). Use them before
issuing a repair call — that is the cost-model win (`docs/WHY.md`).

## Common failure patterns

| Symptom | Likely cause | Next step |
|---|---|---|
| `rejected` + revision text | Stale `expected_revision` | Re-read; merge; resubmit with new revision |
| `rejected` + dry_run / destructive | Security forced dry-run or missing `allow_destructive` | See [`SECURITY.md`](../SECURITY.md); explicit `dry_run: false` only when intentional |
| `partially_completed` on Niagara create | Expected until B10/metrics | Read `capability_notes`; do not claim POC-B |
| `partially_completed` + `job` | Timeout path (ADR-0009) | Poll `get_job_result` |
| MCP cancel returned HTTP 202 but work continues | UE 5.8's private ToolsetRegistry adapter has no `CancelAsync`; 202 proves notification acceptance only | Use UEREMCP `cancel_job(job_id)`, then poll `get_job_result`; see [`limitations.md`](limitations.md) |
| Tool not in `list_toolsets` | Plugin module not loaded / Live Coding | Human: rebuild/enable UEREMCP; agent: Ping Reference |
| `execute_plan` missing as tool | Not AICallable | Use `InstantiateTemplate` or domain tools |

## Metrics fields (do not over-interpret)

Response `metrics` may include `mcp_round_trips`, `internal_operations`,
`replayed`, timing. POC-B token / wall metrics are **partial** —
[`docs/reviews/poc-metrics.md`](../reviews/poc-metrics.md). Missing tokens ≠ success.
