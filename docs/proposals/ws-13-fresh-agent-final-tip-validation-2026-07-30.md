# WS-13 fresh-agent validation — final deploy tip

**Date:** 2026-07-30  
**Deploy / live editor SHA:** `cb07277b759538893079ee6bd0b249997879ea3f`  
**Junction:** unchanged at `$UEREMCP_DEPLOY/Plugins/UEREMCP`  
**Machine result:** [`ws-13-fresh-agent-final-tip-validation-2026-07-30.json`](ws-13-fresh-agent-final-tip-validation-2026-07-30.json)

## Verdict

**8/8 acceptance workflows passed.** A fresh agent that starts with
`docs/guide/tool-selection-policy.md`, the machine contract, catalog routing, and
live list/describe has a clear reason to prefer UEREMCP semantic tools for covered
create/modify/validate goals. It should use Epic tools only for read-only
discovery or catalog gaps.

This prioritization is documented and testable, not forced. `list_toolsets` alone
still presents competing Epic tools whose descriptions call themselves “primary”;
the policy/catalog are therefore load-bearing.

## Calls and outcomes

| Workflow | MCP calls | Outcome |
|---|---:|---|
| list + four relevant describes + Ping | 6 | First success: `no_change_required` |
| Tool-selection benchmark | 0 | 12/12 intents routed to expected UEREMCP tools |
| Blueprint shipped example → discovery → read | 3 | Missing sample rejected honestly; discovered POC scratch read with revision |
| Niagara shipped example → discovery → inspect | 3 | Missing sample rejected fail-soft; discovered FreshAgent system inspected without crash |
| Cold visual capture → poll | 2 | Completed, PNG reread, `max_delta_lit_pixels=1098` |
| Niagara idempotency retry | 1 | `no_change_required`; no mutation |
| Expected visual validation failure | 1 | `rejected`: `options.validate=true` required |
| Cooperative cancel discovery | 1 | Unknown job honestly `rejected`: `job not found` |

One additional MCP call described Epic `AssetTools` before policy-approved
read-only discovery. Total live MCP calls: **18**. No C++ source reading was needed.

## Tool-selection assessment

- **Policy:** direct intent → preferred UEREMCP tool → primitive path to avoid.
- **Catalog:** mirrors the same routing table near the top.
- **Machine contract:** deterministic routing benchmark passed all 12 intents.
- **Live naming:** every UEREMCP domain starts with `Ueremcp`; Niagara and visual
  descriptions explicitly say goal-level/composed operation.
- **Residual:** UHT schemas remain an opaque `requestJson` string, so guide
  examples and the machine contract remain necessary.
- **Residual:** shipped read/inspect sample paths were absent in this RE session.
  Calls failed honestly, but a fresh agent needed two discovery calls.

## Visual / fail-soft evidence

`CaptureEffectFrames` returned a cold `partially_completed` job with
`poll_action=get_job_result` and `cancellable=false`. One poll returned
`no_change_required`, `rendered_something=true`, `png_files_reread=true`,
`stage_teardown_complete=true`, and `max_delta_lit_pixels=1098`.

Evidence:
`RE/Saved/UEREMCP/VfxCapture/NS_FreshAgent_Probe/final-fresh-capture-cb07277/age_01.png`
(157,982 bytes; valid PNG signature and IHDR).

`InspectSystem` rejected the missing published example target without crashing,
then returned `partially_completed` for the discovered FreshAgent system with
honest lossy-topology notes.

## WS-13 fixes

1. Corrected contradictory `ExecutePlan` policy wording: it is registered
   AICallable, but not the preferred first choice.
2. Added the registered `ExecutePlan` tool to the machine inventory.
3. Aligned Ping and job poll/cancel statuses with live/catalog behavior.
4. Clarified that published JSON examples are request shapes, not asset-existence
   guarantees, and documented read-only discovery recovery.
5. Added regression checks for these facts.

## Ready for a new agent

**Yes, within cataloged scopes**, provided the agent starts with the guide routing
policy, treats example target paths as replaceable, honors response status, and
polls the documented cold-capture job. This remains practical POC tooling, not a
claim of production-perfect coverage.
