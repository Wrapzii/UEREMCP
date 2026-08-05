# Proposal: Louder MCP bootstrap (agents still skip GetStarted)

- **From:** WS-01
- **To:** WS-03 (GetStarted/ResolveIntent copy), WS-13 (guide), Cursor agents (done)
- **Date:** 2026-08-03

## Problem

Fresh Cursor agents treat Epic `list_toolsets` / `describe_toolset` as discovery,
then issue many primitive MCP calls (or Python). They rarely call
`GetStarted` → `ResolveIntent` even though those tools exist and return the
full plan + `request_json` in one shot.

This is not a missing capability — it is discoverability vs Cursor's generic MCP
browser surface. Policy docs already say START HERE; LLMs still default to
list_toolsets.

## Done here (WS-01)

- Added always-apply Cursor rule `.cursor/rules/mcp-bootstrap.mdc`
- Pointed `.cursor/rules/uneremcp.mdc` at it

## Ask WS-03

In `FUeremcpIntentRouter::GetStarted` / Reference toolset UFUNCTION comments:

1. Lead with **DO NOT call list_toolsets first**
2. Put a worked `ResolveIntent` example in the first capability_note
3. Optionally return a `forbidden_first_calls: ["list_toolsets"]` field in the
   GetStarted payload so agents that do call GetStarted see a hard demotion

## Ask WS-13

Mirror the same "banned first discovery" language in
`docs/guide/tool-selection-policy.md` §1 without claiming forceability.

## Honest limit

No in-repo rule can guarantee arbitrary LLM tool choice. Cursor rules + louder
GetStarted are the available levers short of hiding Epic mutators via
`SetNameFilters` (separate ADR/proposal).
