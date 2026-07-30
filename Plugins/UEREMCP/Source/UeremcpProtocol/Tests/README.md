# UeremcpProtocol tests

**Owner:** WS-05.

## Outside-editor Python (regression / golden generator)

```bash
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py/run_tests.py
```

Covers unit tests plus `test_golden.py`, which asserts against `Tests/golden/`.

**Python green is not C++ parity** (WS-14 C-2). See `../Docs/CPP_PARITY.md`.

## Golden vectors (`Tests/golden/`)

Fixed JSON inputs + expected outputs shared by Python and C++:

| Suite | Files |
|---|---|
| envelope | `request.in.json`, `request.parsed.expected.json`, `response_fields.in.json`, `response.out.expected.json` |
| content_hash | `graph.in.json`, `graph_cosmetic.in.json`, `hash.expected.txt`, `canonical.expected.json` |
| ref | `spec.in.json`, `completed.in.json`, `resolved.expected.json` |
| topo | `nodes.in.json`, `order.expected.json` |

Regenerate expected outputs (then re-run both sides):

```bash
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/golden/generate_goldens.py
```

## C++ AutomationTests (production `FUeremcp*`)

Source: `Private/Tests/UeremcpProtocolGoldenTests.cpp`  
Filter prefix: `UEREMCP.Protocol.Golden`

```bat
UnrealEditor-Cmd.exe "<Project>.uproject" -unattended -NullRHI -nop4 ^
  -ExecCmds="Automation RunTests UEREMCP.Protocol.Golden;Quit"
```

Optional: `set UEREMCP_PROTOCOL_GOLDEN_ROOT=<abs path to Tests/golden>`

Do **not** claim C++/Python parity until these AutomationTests pass.

ADR-0009 registry tests live in
`Private/Tests/UeremcpJobRegistryTests.cpp` with filter prefix
`UEREMCP.Protocol.JobRegistry`. They cover lifecycle/poll metrics, cooperative
cancellation, bounded capacity, concurrent polls, active/terminal expiration,
and the initiating timeout envelope.

ADR-0008 interpreter tests live in
`Private/Tests/UeremcpPlanExecutorTests.cpp` with filter prefix
`UEREMCP.Protocol.PlanExecutor`. They cover fail-closed preflight, stable
dependency dispatch, response `$ref` substitution, consolidated metrics/change
results, one-call accounting, and confirmed cross-operation rollback.

Offline Python mirror: `Tests/py/test_plan_executor.py` (same semantics;
includes continue_independent, optional failures, cycles, and atomic preflight).

ADR-0006 idempotency tests live in
`Private/Tests/UeremcpIdempotencyTests.cpp` with filter prefix
`UEREMCP.Protocol.Idempotency`. They cover replay annotation, durable reload from
`Saved/UEREMCP/idempotency/` (temp override), and the accepted durable root.
