# Audit: agent-facing execute_plan (WS-05)

**Owner:** WS-05 (Protocol Docs; matrix rows remain WS-02-owned under `docs/audit/`)  
**Date:** 2026-07-30  
**Action:** `execute_plan`

## Existing equivalents

| Source | Tool | What it does | Why insufficient for UEREMCP |
|---|---|---|---|
| Epic EditorToolset | `ProgrammaticToolset.execute_tool_script` | Batches other registered tools via sandboxed Python `run()` | Imperative script glue, not one ADR-0003 envelope; no consolidated verification/manifest contract. **Preserve and compose under plan ops** — do not reinvent `[VERIFIED: docs/audit/epic-toolsets.md; programmatic.py cited in GROUNDED_FACTS]`. |
| REAgentTools | `batch_workflow` / `execute_editor_batch` | Declarative allowlisted ops + `$ref` chaining; single editor transaction | 8-action allowlist; RE-specific surface; not goal-level domain actions; not ADR-0003 envelope. **Supersede surface; preserve `$ref` / dry_run / stop_on_error ideas** `[VERIFIED: docs/audit/reagenttools.md; VERIFIED-RUNTIME: describe_toolset 2026-07-30]`. |
| Epic | `UAgentSkillToolset` | Prompt-shaped skill CRUD | Not execution-shaped plans (ADR-0008). Preserve; do not subclass. |

## UEREMCP disposition

- Agent-facing action: `execute_plan` with `specification` =
  `schemas/batch/plan.schema.json`.
- Interpreter: `FUeremcpPlanExecutor` (fail-closed; domain handlers register in).
- Public string adapter: `FUeremcpPlanActions::ExecutePlan` (this module).
- AICallable wrapper: Core `UUeremcpReferenceToolset::ExecutePlan` (WS-03;
  proposal `docs/proposals/ws-05-execute-plan-aicallable.md`).

Adding this action does **not** duplicate a working Epic goal-level plan tool;
it supersedes the REAgentTools batch *surface* while composing Epic's
`execute_tool_script` as an optional nested primitive for domains that need it.
