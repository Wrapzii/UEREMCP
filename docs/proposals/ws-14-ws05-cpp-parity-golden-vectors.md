# Proposal: C++/Python golden-vector parity for UeremcpProtocol

- **From:** WS-14
- **To:** WS-05, WS-11
- **Date:** 2026-07-29
- **Severity:** Critical (handoff artifact incomplete)

## Finding

`Plugins/UEREMCP/Source/UeremcpProtocol/Tests/README.md` lines 21–22 require C++ to
match Python. All 32 tests execute only the Python package `ueremcp_protocol/`. There are
**no C++ automation tests** and no golden-vector file comparing canonical bytes from
`FUeremcpContentHash` vs `content_hash.py`.

`docs/WORK_ALLOCATION.md` handoff: "envelope round-trip in C++" — code exists but parity
is unverified.

## Ask

1. Add `Tests/golden/` with fixed JSON inputs and expected outputs for:
   - envelope parse + serialize round-trip
   - `content_hash` canonical bytes
   - `$ref` resolution (object + dollar-string)
   - topological sort
2. Python tests: assert against golden files (regression guard).
3. C++ `AutomationTest` module (can be WS-11 harness if WS-05 stays editor-free): load
   same golden files, assert C++ output matches.
4. Optionally: one test that validates parsed envelope against
   `schemas/envelope/request.schema.json` via a shared validator to catch hand-rolled
   drift in `UeremcpEnvelope.cpp`.

## Acceptance

WS-14 will clear C-2 in `docs/reviews/wave-1-2026-07-29.md` when golden vectors pass
in both languages in CI or documented local run.

## Response

**Accepted — Critical handoff incompleteness.** Python-only green is not C++
parity. WS-05 owns golden vectors + Python regression; C++ AutomationTest may
live under WS-05 protocol Tests or be co-owned with WS-11 harness, but must
exercise `FUeremcp*` production code, not a second mirror.

Envelope round-trip in C++ is not done until golden vectors match.

### Update 2026-07-29 (partial)

WS-05 `0f91f91`: `Tests/golden/` + Python `test_golden.py` (38 OK) + C++
`UEREMCP.Protocol.Golden*` AutomationTests over `FUeremcp*`. Parity **not
claimed** until editor AutomationTests pass (`Docs/CPP_PARITY.md`). Blocked on
same shipping load issue as C-3: `UeremcpProtocol.dll` must build/link (WS-03).
