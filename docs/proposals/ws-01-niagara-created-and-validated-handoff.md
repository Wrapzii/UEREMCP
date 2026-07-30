# WS-01 handoff: Niagara `created_and_validated` completion

- **From:** WS-01
- **To:** WS-07
- **Priority:** POC-B residual
- **Claim lineage:** `2aab525`
- **Date:** 2026-07-30

## Problem

The live POC-B MCP request passes B1–B10, but its response remains
`partially_completed`. The current result bundle preserves that value correctly;
WS-01 does not relabel it after the fact.

This is now a product-status ceiling: callers cannot distinguish the fully proven
fireball outcome from genuinely incomplete work by reading `status`.

## Requested behavior

Return `created_and_validated` for Niagara create when the same operation has
positively established all of these gates:

1. structural re-read passes for all requested emitters and exposed parameters;
2. renderer bindings and required material create/reuse dispositions pass;
3. compilation was genuinely awaited and completed successfully;
4. save and change-manifest checks pass;
5. visible-render validation passes the programmatic particle and warm-pixel gates;
6. no required validation check is skipped, deferred, timed out, or warning-only.

Keep `partially_completed` whenever any required gate is deferred or unavailable.
Keep `failed_validation` for completed validation that finds a defect. Do not infer
validated completion merely because creation returned without error.

## Acceptance test

Add WS-07-owned tests with the implementation:

- a fully passing create returns `created_and_validated`;
- each missing/deferred gate prevents `created_and_validated`;
- a failing completed gate returns `failed_validation`;
- the response still contains the complete change manifest and honest validation
  diagnostics.

WS-11 should then re-run the live POC-B request and refresh the evidence bundle.
Until that runtime proof exists, the canonical POC-B claim continues to report the
observed `partially_completed` status as an open residual.
