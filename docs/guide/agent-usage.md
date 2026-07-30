# Agent usage

**Owner:** WS-13. Readers: agent clients (Composer, Claude, Grok, Cursor MCP).

This page is self-contained for the call loop. Schema field definitions live in
[`schemas/envelope/request.schema.json`](../../schemas/envelope/request.schema.json)
and [`schemas/common/defs.schema.json`](../../schemas/common/defs.schema.json). Do not
re-derive them.

## 1. Discovery

UEREMCP rides Epic's in-editor MCP server (ADR-0002). Runtime discovery is Epic's
surface — not a UEREMCP invent:

| Epic tool | Use |
|---|---|
| `list_toolsets` | See registered toolsets (`UeremcpBlueprint`, `UeremcpNiagara`, …) |
| `describe_toolset` | List `AICallable` tools and parameter schemas |
| `call_tool` | Invoke one tool with its JSON argument(s) |

Semantic action names (`read_graph`, `create_niagara_effect`, …) are the
`action` field **inside** the request envelope. MCP tool names are PascalCase
methods on the toolset (e.g. `ReadGraph`, `CreateNiagaraEffect`). Map them via
[`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md).

UEREMCP discovery actions (`list_domains`, `describe_action`, …) are still
**planned** in the catalog — do not wait for them.

Reachability smoke (no assets):

```json
{ "toolset": "UeremcpReference", "tool": "Ping" }
```

Or `Echo` with a minimal envelope (see §2).

## 2. Request envelope

Every mutating / domain tool takes **one** `RequestJson` string conforming to
`schemas/envelope/request.schema.json`. Required top-level fields:

| Field | Role |
|---|---|
| `protocol_version` | `"1.0"` — mismatched MAJOR is rejected |
| `action` | Goal-level verb selecting the `specification` schema |

Common optional fields you should use deliberately:

| Field | Role |
|---|---|
| `request_id` | Correlation; echoed on the response |
| `project.path` / `engine_version` | Must match the open project when path policy runs |
| `target.asset_path` | Soft path under `/Game/...` (not a filesystem path) |
| `mode` | Collision semantics (`create`, `create_or_update`, `replace`, …) — ADR-0006 |
| `specification` | **Only** domain-extensible object |
| `options.*` | `dry_run`, `validate`, `timeout_ms`, `response_detail`, … |
| `expected_revision` | Optimistic concurrency (ADR-0006) |
| `idempotency_key` | Retry dedupe (ADR-0006) |

Minimal probe:

```json
{
  "protocol_version": "1.0",
  "request_id": "agent-echo-1",
  "action": "echo",
  "specification": {}
}
```

Full fireball-shaped create request (do not paste giants into prompts — load the
fixture): [`schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json`](../../schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json)
→ field `request`.

Response envelope: [`schemas/envelope/response.schema.json`](../../schemas/envelope/response.schema.json).
Always read `status`, `summary`, then `validation` / `result` / `diagnostics` as needed.
Never treat "tool call returned" as success — see [`troubleshooting.md`](troubleshooting.md).

### `response_detail`

| Value | Payload |
|---|---|
| `minimal` | status + primary asset |
| `summary` (default) | change manifest, validation, metrics — no raw object dumps |
| `diagnostic` | + full diagnostics / execution trace |
| `complete` | + full graph payloads |

Cost model (`docs/WHY.md`): prefer one richer response over a second round trip.
Default to `complete` when you will need the graph next.

## 3. One semantic operation

Submit a **complete intended outcome** in one call. Do not:

- inspect → mutate → inspect pin-by-pin with Epic primitives
- call UEREMCP just to wrap one Epic node edit
- trim useful adjacent context from a follow-up you already know you need

Internal primitives may exist; they are not the agent contract. Prefer catalog
actions marked `available` or `partial` with documented notes.

## 4. `dry_run`

Envelope default for ordinary create/update is `options.dry_run: false`
(cost model — dry runs that skip real code predict nothing).

Security policy **forces** dry-run unless the request explicitly sets
`dry_run: false` when the operation is destructive (`mode: delete`, replace of an
existing asset, predicted deletes, …). See [`docs/SECURITY.md`](../SECURITY.md).

Pattern: plan destructive work with an explicit dry run first; only then opt out.

## 5. `idempotency_key`

On completion the service may store `(idempotency_key → response)`. A repeat within
the retention window returns the **stored** response with `metrics.replayed: true`
and performs no work (ADR-0006).

Use a stable key per logical attempt. Changing `request_id` alone does **not** create
a new attempt if the idempotency key is reused.

## 6. `expected_revision`

Graph / asset reads return `revision` / `content_hash`. On modify, send
`expected_revision` from the read you based the edit on.

Default `options.on_revision_conflict: reject` → `status: rejected` with current
revision (no silent last-writer-wins). Omit only for pure creates or when you accept
the risk in a single-agent session.

## 7. Timeout and poll (ADR-0009)

| `options.timeout_ms` | Behaviour |
|---|---|
| `0` or omitted | Complete inline on the open `tools/call` SSE stream |
| `> 0` | If still running at timeout → `status: partially_completed` + `job` handle; work continues in-process |

Poll with Reference toolset `GetJobResult` (`action: get_job_result`,
`specification.job_id`). For jobs that advertise `cancellable: true`, cancel
cooperatively via `CancelJob` (`action: cancel_job`) and poll until terminal
`job.state: cancelled`. This scheduler path is editor-verified. It is **not** the
same as MCP `notifications/cancelled`, which cannot reach ToolsetRegistry/AICallable
work through UE 5.8's private adapter (see
[`limitations.md`](limitations.md)).

`metrics.mcp_round_trips` counts poll calls. Do not pretend a polled job was one trip.

Default long-op timeout guidance in ADR-0009 is **120000** ms; treat ~30s of silent
SSE as a client risk even when the envelope default is higher.

## 8. Scratch roots

Prefer `/Game/__UeremcpTests/` (and POC paths under `/Game/__UeremcpPoc/` only when
running an accepted POC harness). Do not write into production content paths unless
the human explicitly asked.
