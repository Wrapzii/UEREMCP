# WS-11 handoff: POC B transport, visible render, and metrics

**Status:** B1/B6 closed; metrics evidence advanced but incomplete  
**Observed orch tips:** `29b4b06`, `d07f8f1`, `cd08c64`  
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

## Successful transport evidence

The successful run used one `CreateNiagaraEffect` call with all six components,
all six inline material specifications, and `compile`, `validate`, and `save`
enabled. Its structured response records:

- `metrics.mcp_round_trips = 1`
- `metrics.internal_operations = 46`
- `poc_b_gates.B1_single_request_complete = true`
- `poc_b_gates.B6_compile_awaited = true`
- six material assets in `result.reused_assets`

The editor log records dispatch at `2026-07-30 11:14:53.492` and completion of
the final synchronous content-validation record at `11:14:55.811`. The observed
server-side interval is therefore **2.319 seconds**. This is a reproducible lower
bound, not wall-clock completion: the log has no response-write marker and the
calling harness did not retain a client start/end timestamp. It must not be copied
into `wall_clock_seconds`.

Evidence:
`$UEREMCP_LEGACY_PROJECT/Saved/Logs/RE-backup-2026.07.30-11.18.22.log`,
lines 3034-3075. [VERIFIED-RUNTIME: one-call MCP run on orch `d07f8f1`]

## Token applicability

The run was issued through Cursor's MCP caller, which did not expose per-call
agent usage. Consequently:

- total input/output tokens are **unavailable**, not zero;
- `wire_bytes / 4` from the historical REAgentTools harness is only an estimated
  payload-token proxy, not total agent tokens;
- the response body size cannot recover prompt, cache-read, retry, or surrounding
  session usage after the fact.

A token result requires a fresh isolated trial using an agent harness that reports
input and output usage for the complete goal. If that facility remains unavailable,
WS-14 should record `unavailable (harness does not expose usage)` rather than a
number. [VERIFIED:
`REAgentTools/Docs/BENCHMARK_REPORT.md:6-15`]

## Equivalent primitive baseline: negative finding

No measured fireball-equivalent primitive baseline exists in
`REAgentTools/Docs/benchmark_ab_live.json`. Its measured scenarios are actor spawn
and batch movement only. The Niagara guidance recommends one
`ProgrammaticToolset.execute_tool_script` for system authoring, so its single MCP
round trip is a batch container and is not a measured primitive-operation count.
[VERIFIED:
`REAgentTools/Docs/NIAGARA_BATCHING.md:1-18`]

Do not substitute any of these as the POC B baseline:

- the owner-reported approximately `5:1` general efficiency;
- UEREMCP's `46` internal operations;
- a hand-count inferred from six emitters or materials;
- REAgentTools' one script call.

### Reproducible baseline handoff

WS-14 needs a fresh baseline trial against the same starting state and acceptance
checks as the UEREMCP run:

1. Reset `/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline` and its generated
   dependencies.
2. Use only pre-UEREMCP Epic/REAgentTools tools.
3. Build the same six roles and six material specifications, expose colour, scale,
   and intensity, await compile, save, then re-read for structural verification.
4. Count every primitive operation executed inside any programmatic batch as well
   as every MCP round trip; retain failures and retries.
5. Capture client monotonic start/end time and the harness-reported total token
   usage.
6. Repeat at least three trials from clean state and preserve raw per-call records.

WS-07 must identify the exact Epic primitive/tool sequence because Niagara domain
ownership is required to establish semantic equivalence. WS-11 can execute and
validate the trials once that sequence and a token-reporting harness are available.

## Metrics status

The acceptance contract requires measured `mcp_round_trips`,
`internal_operations`, total tokens, wall-clock time, and an equivalent
primitive-call count. [VERIFIED: docs/POC_ACCEPTANCE.md:17-25]

The editor-only B8 evidence reported `mcp_round_trips: 1`,
`internal_operations: 10`, `tokens_total: 0`, and
`primitive_call_equivalent: 1`, but those values describe restart verification,
not a successful B1 create, and must not be reused as POC B creation metrics.

### WS-14 request

Create `docs/reviews/poc-metrics.md` with only the two measured values above and
the 2.319-second server-side lower bound clearly separated from wall-clock time.
Leave total tokens, wall-clock time, and primitive baseline open until the
instrumented trials above run. WS-14 owns `docs/reviews/**`, so WS-11 does not edit
that path. [VERIFIED: docs/WORK_ALLOCATION.md:34-41]

## Honest completion status

- B1/B6: **PASS** — one call, compile genuinely awaited
- measured metrics: `mcp_round_trips=1`, `internal_operations=46`
- timing evidence: **2.319-second server-side lower bound only**
- total tokens: **OPEN** — caller exposed no usage; not zero
- wall-clock time: **OPEN** — client timestamps were not captured
- primitive baseline: **OPEN** — no equivalent measured Niagara scenario exists
- Overall POC B: **NOT CLAIMED**
