# Batch `$ref` substitution grammar

**Owner:** WS-05  
**Status:** Final (2026-07-29)  
**Decision record:** `docs/proposals/ws-05-batch-ref-grammar.md`  
**Schema:** `schemas/batch/plan.schema.json`, `schemas/domains/_shared/result_ref.schema.json`

## Two accepted forms (no third grammar)

### 1. Object form (canonical)

```json
{ "$ref": "material.result.primary_asset" }
```

- Exactly one key: `$ref`
- Value: `<operation_id>.<dotted.path>` into that operation's completed **response**
- Path segments may be object keys or numeric array indices
- Used in UEREMCP fireball-style plans where the envelope `result.*` tree matters

### 2. Dollar-string form (REAgentTools prior art)

```json
"$material"
```

- A JSON string starting with `$` followed by an operation id only
- `[VERIFIED: REAgentTools/.../batch_workflow_tools.py:37-48]`
- Resolves to, in order: `result.primary_asset` → `label` → `path`
- Compatibility with `execute_editor_batch` step_results and UEREMCP envelopes

## Failure

Unresolved / malformed refs **fail the operation**. Never substitute `null`.

## Composition

`execute_plan` composes **over** Epic `ProgrammaticToolset.execute_tool_script`
(preserve) `[VERIFIED: WS-02 docs/audit/epic-toolsets.md q7]`. Epic has no `$ref`
grammar; chaining lives in UEREMCP.

## Implementation

| Layer | API |
|---|---|
| C++ | `FUeremcpRefResolve::ResolveInPlace` |
| Python tests | `ueremcp_protocol.ref_resolve.resolve_refs` |
