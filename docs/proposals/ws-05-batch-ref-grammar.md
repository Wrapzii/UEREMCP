# Decision: batch `$ref` grammar (unblocked)

- **From:** WS-05
- **To:** WS-01, WS-02, domain workstreams
- **Date:** 2026-07-29
- **Status:** decided — supersedes `ws-05-batch-grammar-blocked.md`
- **Schema:** `schemas/batch/plan.schema.json` — **final for `$ref` grammar and composition**

## Evidence

### Epic — compose, do not replace

`ProgrammaticToolset.execute_tool_script` **exists** and is the engine batching
primitive. Disposition: **preserve** — `execute_plan` composes over it for
Niagara/editor multi-tool orchestration.

- `[VERIFIED: UEREMCP-ws02/docs/audit/epic-toolsets.md q7]`
- `[VERIFIED: UEREMCP-ws02/docs/audit/raw/q7-programmatic-execute-tool-script.json]`
- Input: Python `script` defining `run() -> dict`; sandbox exposes `execute_tool`
  by dotted tool name + JSON string
  `[VERIFIED: programmatic.py:904-953 per WS-02 raw dump]`

Epic does **not** define a result-chaining `$ref` grammar. That comes from
REAgentTools + UEREMCP's envelope-shaped responses.

### REAgentTools — dollar-string prior art

`execute_editor_batch` resolves string values that start with `$`:

```python
# [VERIFIED: REAgentTools/.../batch_workflow_tools.py:37-48]
def _resolve_ref(step_results, ref: str) -> str:
    if not ref.startswith("$"):
        return ref
    step_id = ref[1:]
    # → step_results[step_id]["label"] or ["path"]; raises if unknown / missing
```

Also: 8 allowlisted actions; `dry_run`; `stop_on_error`
`[VERIFIED: batch_workflow_tools.py:17-26, 66-70]`. Failure does not substitute
null — raises `ValueError`.

## Decision: support **both** existing grammars

| Form | Shape | Source | Use |
|---|---|---|---|
| **Object (canonical)** | `{"$ref": "<op_id>.<dotted.path>"}` | UEREMCP plan schema / envelope | Rich paths into ADR-0003 responses, e.g. `material.result.primary_asset` |
| **Dollar-string (compat)** | `"$<op_id>"` | REAgentTools `_resolve_ref` | Shorthand for the prior op's primary identity |

**Not adopted:** a third unrelated grammar (e.g. JSON Pointer, Jinja, Epic script
locals as the agent-facing contract).

### Dollar-string resolution order for `$op_id`

Against that operation's completed response / step bag:

1. `result.primary_asset` if a non-empty string (UEREMCP envelope)
2. `label` if a non-empty string (REAgentTools step_results)
3. `path` if a non-empty string (REAgentTools step_results)
4. **fail** the operation — never substitute null

### Shared failure rule (both forms)

Resolution failure fails the operation. Silently-null asset paths produce broken
assets that still "validate" — prohibited (existing plan schema `$comment`).

### Composition with Epic

```
agent  →  execute_plan (UEREMCP, depends_on + $ref)
              ├─ domain goal tools (create_niagara_effect, …)
              └─ may invoke ProgrammaticToolset.execute_tool_script
                 for engine multi-tool scripts (preserve)
```

`execute_plan` is the agent-facing batch; `execute_tool_script` remains available
internally / for script-shaped orchestration. Do not reimplement its sandbox.

## What stays open (not grammar)

- Per-domain action allowlists inside plans (domains register actions)
- Wiring the executor to actually call `execute_tool_script` (WS-03/Core + domains)
- Formal REAgentTools matrix row if WS-02 is still finishing prose — grammar
  evidence above is sufficient from source

## Schema / code touchpoints

- `schemas/batch/plan.schema.json` — final `$ref` grammar
- `schemas/domains/_shared/result_ref.schema.json` — both forms
- `FUeremcpRefResolve` + Python `ueremcp_protocol.ref_resolve`
- `Plugins/UEREMCP/Source/UeremcpProtocol/Docs/BATCH_REF.md`
## Response

**Accepted.** Dual grammar is the right call:

- Object-form remains canonical for envelope-shaped paths.
- Dollar-string remains REAgentTools-compatible shorthand with a defined
  resolution order (`primary_asset` → `label` → `path` → fail).
- No third grammar.
- `execute_plan` composes over `execute_tool_script` (preserve).

`schemas/batch/plan.schema.json` is **frozen for `$ref` grammar**. Executor
wiring and per-domain allowlists stay open as noted — not grammar work.
`ws-05-batch-grammar-blocked.md` is superseded by this decision.
