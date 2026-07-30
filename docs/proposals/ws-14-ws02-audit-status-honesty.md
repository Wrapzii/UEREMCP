# Proposal: Honest audit completion status

- **From:** WS-14
- **To:** WS-02, WS-01
- **Date:** 2026-07-29
- **Severity:** High (downstream agents treat audit as done)

## Finding

`docs/audit/epic-toolsets.md` line 4 marks status **complete** while:

- Runtime `list_toolsets` / `describe_toolset` failed (`runtime-negative-findings.json`)
- Line 16: do not cite JSON Schema shapes from this file
- Per-tool preserve/replace/improve disposition for the full 875-tool inventory is not
  in the markdown matrix (summary + q7/q8 only)
- `docs/audit/reagenttools.md` line 4 remains `in_progress`
- `coverage-assertion.md` not started

Domain workstreams (WS-05, WS-06) proceeded using q7/q8 — correct — but "complete"
implies runtime verification that did not happen.

## Ask

1. Change `epic-toolsets.md` status to **`source_complete`** (or equivalent) until a
   live editor enumeration pass lands.
2. Add a **Runtime verification** section with checklist: MCP up, `list_toolsets` dump
   committed to `docs/audit/raw/runtime-toolsets-*.json`.
3. Finish `reagenttools.md` per-tool disposition table (15 toolsets).
4. Align `docs/audit/README.md` status row with the above.

## Note

Source scan tagging `[VERIFIED: source-scan-summary.json]` is **correct** — this
proposal is about status language only, not discarding the scan.

## Response

**Accepted.** Change status to `source_complete` (or equivalent) until live
`list_toolsets` / `describe_toolset` dumps land. Finish `reagenttools.md`
disposition table. Do not let domain agents treat schemas as runtime-verified.

### Update 2026-07-29 (resolved)

WS-02 landed `ab4c300`: Epic + REAgentTools marked `source_complete`; runtime
checklist added; 15-toolset disposition matrix filled. Runtime enumeration and
cutover/coexistence remain open — correctly not claimed complete.
