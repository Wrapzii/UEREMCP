# WS-01 — Ready for fresh-agent test (2026-07-30)

**Owner:** WS-01  
**Status:** Ready for user validation  
**Final main SHA:** this commit (resolve exactly with `git rev-parse main`)  
**Integrated content parent:** `90fe0bf`  
**Deploy worktree:** `$UEREMCP_DEPLOY`  
**RE plugin junction:** `$UEREMCP_LEGACY_PROJECT\Plugins\UEREMCP`
→ `$UEREMCP_DEPLOY\Plugins\UEREMCP`

The fresh-agent usability and final-tip validation commits are integrated on local
`main`. The integration is documentation, schema, examples, and captured validation
evidence only; it does not change C++ and therefore does not require a plugin rebuild.

## Fresh-agent startup

1. Read [`docs/guide/tool-selection-policy.md`](../guide/tool-selection-policy.md).
2. Read [`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) and route only to
   capabilities whose current status and limitations fit the requested goal.
3. Call Epic MCP `list_toolsets`; use the exact registered full toolset names it
   returns.
4. Prefer `Ueremcp*` goal-level semantic tools for create, modify, and validate
   workflows. Use Epic tools for read-only discovery or a cataloged UEREMCP gap.
5. Use `describe_toolset` before the first call when the method or argument schema is
   uncertain, then submit one complete semantic request rather than a primitive
   inspect/mutate loop.
6. Judge success from the response status plus validation evidence, not from MCP call
   completion alone.

Machine-readable routing is in
[`docs/guide/tool-selection-contract.json`](../guide/tool-selection-contract.json).
Fresh-agent reports are:

- [`ws-13-fresh-agent-usability-validation-2026-07-30.md`](ws-13-fresh-agent-usability-validation-2026-07-30.md)
- [`ws-13-fresh-agent-final-tip-validation-2026-07-30.md`](ws-13-fresh-agent-final-tip-validation-2026-07-30.md)

Visual capture request documentation and schemas are linked from the capability
catalog and [`docs/guide/capability-reference.md`](../guide/capability-reference.md).
