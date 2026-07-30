# WS-11 handoff: POC B transport, visible render, and metrics

**Status:** B1/B6 closed; B10 harness pending orch build; metrics incomplete  
**Observed orch tips:** `29b4b06`, `d07f8f1`  
**Owners requested:** WS-14 (metrics record)

## Update after `d07f8f1`

The canonical one-call MCP fireball now returns without crashing:

- `metrics.mcp_round_trips`: `1`
- `metrics.internal_operations`: `46`
- `poc_b_gates.B1_single_request_complete`: `true`
- `poc_b_gates.B6_compile_awaited`: `true`
- six material assets present in `result.reused_assets`

B1 and B6 are closed. The response remains `partially_completed` because B10 and
other non-B1 validation slices remain explicitly skipped.

WS-11 added `UEREMCP.Niagara.POCB.VisibleRender`, which places the canonical
fireball in the active editor viewport, saves a supplementary PNG, and requires
both a minimum pixel delta and a warm-colour fire signature. It restores the
viewport and destroys the transient actor. Runtime status remains **unproven**
until orch lands, rebuilds Validation, and runs the filter with rendering enabled.

Metrics remain incomplete: the response provides MCP round trips and internal
operations, but not total tokens or wall-clock time, and the equivalent primitive
workflow/count has not been measured. WS-14 still owns
`docs/reviews/poc-metrics.md`.

## B1 transport result

WS-11 started the real RE editor with the orch junction unchanged, confirmed the
UEREMCP Niagara toolset over Streamable HTTP MCP, and issued exactly one
`CreateNiagaraEffect` tool call. The request used:

- request id `ws11-poc-b1-mcp-29b4b06`
- target `/Game/__UeremcpPoc/NS_POCB_Fireball_MCP`
- all six roles (`core`, `flame_shell`, `sparks`, `smoke`, `ribbon_trail`,
  `impact_burst`)
- all six inline material specifications
- `compile`, `validate`, and `save` enabled

The editor crashed before returning an MCP response:

```text
Assertion failed: IsValid()
FUeremcpNiagaraCreate::Run()
UeremcpNiagaraCreate.cpp:589
UUeremcpNiagaraToolset::CreateNiagaraEffect()
```

Editor log:
`$UEREMCP_LEGACY_PROJECT/Saved/Logs/RE.log`,
timestamps `2026-07-30 10:52:14` through `10:52:22`.

The failing line calls `AwaitCompile(System, Context, TimeoutSeconds,
CompileState)`. [VERIFIED:
Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraCreate.cpp:585-590]

**Result:** B1 FAIL. A real one-request MCP transport call was made, but no
structured response was returned. The successful editor automation filter is not
a substitute for this criterion.

### WS-07 request

Fix or guard the invalid shared pointer reached by the MCP-dispatched compile-await
path, then ask WS-11 to repeat the same one-call request. The rerun must retain
`compile:true`, `validate:true`, and `save:true`; disabling validation is not an
acceptable POC result.

## B10 visible-render status

No executable visible-render validation was produced in this run. Existing
structural/material/compile/restart gates establish B2-B9, but do not establish
that the placed system visibly renders as a fireball. A screenshot by itself is
supplementary evidence and cannot close B10.

### WS-07 request

Define and implement the domain-owned runtime placement/render validation needed
for B10. WS-11 can then own the editor automation wrapper and evidence extraction.
The handoff should identify the machine-checkable condition that accompanies any
screenshot (for example, the domain-approved runtime/render observation); WS-11
must not invent that condition from screenshots.

## Metrics status

POC B metrics are not recordable from this attempt because the transport operation
crashed without returning a response. The acceptance contract requires measured
`mcp_round_trips`, `internal_operations`, total tokens, wall-clock time, and an
equivalent primitive-call count. [VERIFIED:
docs/POC_ACCEPTANCE.md:17-25]

The editor-only B8 evidence reported `mcp_round_trips: 1`,
`internal_operations: 10`, `tokens_total: 0`, and
`primitive_call_equivalent: 1`, but those values describe restart verification,
not a successful B1 create, and must not be reused as POC B creation metrics.

### WS-14 request

After B1 and B10 pass, record the measured POC B values in
`docs/reviews/poc-metrics.md` and document the equivalent primitive workflow used
for the baseline comparison. The repository's current general baseline is the
owner-reported approximately 5:1 REAgentTools efficiency, not a measured fireball
primitive count. [VERIFIED: docs/WHY.md:14-17]

## Honest completion status

- B1: **FAIL** — editor crash, no MCP response
- B10: **OPEN** — no machine-checkable visible-render proof
- POC B metrics: **BLOCKED** — successful transport timing/operation counts and
  primitive baseline unavailable
- Overall POC B: **NOT CLAIMED**
