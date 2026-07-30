# Proposal: batch $ref grammar blocked on WS-02 audit

- **From:** WS-05
- **To:** WS-02, WS-01
- **Date:** 2026-07-29
- **Status:** blocked — no schema finalisation

## What is blocked

Finalising `schemas/batch/plan.schema.json` `$ref` substitution grammar and the
batch executor's allowlist / failure semantics.

## Why

`plan.schema.json` already documents an object-form `$ref`:

```json
{ "$ref": "<operation_id>.<dotted.path.into.that.operation's.response>" }
```

REAgentTools `execute_editor_batch` uses a **different** prior-art form: string
values prefixed with `$` that resolve to a prior step's `label` or `path`
(`[VERIFIED: $RAT/.../batch_workflow_tools.py:_resolve_ref]`). Epic's
`ProgrammaticToolset.execute_tool_script` is still un-audited in
`docs/audit/epic-toolsets.md` (seed only).

Inventing a third grammar, or freezing the schema comment without that audit,
violates AGENTS.md rule 2 and the WS-05 launch brief.

## What WS-05 shipped anyway (independent pieces)

- Envelope parse / serialise / validate
- Dependency topological sort (`depends_on`)
- Provisional object-form `$ref` resolver matching the **current** schema
  `$comment` (not claimed final)
- Deterministic `content_hash` (see protocol `Docs/CONTENT_HASH.md`)
- Session idempotency store (in-memory)

## What WS-02 needs to deliver

From `docs/audit/reagenttools.md` and RB-15:

1. Exact `execute_editor_batch` `$ref` grammar, failure modes, dry_run behaviour
2. Whether Epic `execute_tool_script` is the batching primitive to compose with
3. Disposition: preserve / replace / improve for batching

Until then, `plan.schema.json` stays provisional; WS-05 will not change its
`$ref` shape.

## Response

**Accepted — stay blocked.** Correct call. Do not invent a third `$ref` grammar.
WS-02's audit of REAgentTools `execute_editor_batch` and Epic
`execute_tool_script` remains the unblocker. Envelope / hash / topo-sort /
provisional object-form resolver may ship as Wave 1 independent pieces.

### Update 2026-07-29 (RB-02 landed)

**q7 unblocked:** `ProgrammaticToolset.execute_tool_script` exists — compose
`execute_plan` over it (`docs/audit/epic-toolsets.md`,
`docs/audit/raw/q7-programmatic-execute-tool-script.json`). Disposition:
`preserve`.

**REAgentTools `$ref` still needed for grammar freeze:** WS-02 did not fill
`reagenttools.md` / RB-15 this pass. Prior art (for WS-05 to read, not invent):
`$RAT/.../batch_workflow_tools.py` — string refs prefixed with `$` resolve to a
prior step's `label` or `path` via `_resolve_ref`; 8 allowlisted actions;
`dry_run` / `stop_on_error`. Formal audit row still assigned to WS-02.

### Update 2026-07-29 (RB-15 q6 landed — `dac8d57`)

**Fully unblocked for grammar decision.** Formal dump:
`docs/audit/raw/q-reagenttools-execute-editor-batch.json` +
`docs/audit/reagenttools.md`.

Summary for WS-05:
- `$` + step id → prior `label`, else `path`
- 8 allowlisted actions; `BATCH_LIMIT=20`; `dry_run` (resolve_actor still runs);
  `stop_on_error` default true
- Disposition: preserve grammar ideas; supersede REAgentTools surface; compose
  Epic multi-tool batches via `execute_tool_script`

WS-05 should now choose object-form / `$`-string / both and update
`schemas/batch/plan.schema.json` accordingly.
