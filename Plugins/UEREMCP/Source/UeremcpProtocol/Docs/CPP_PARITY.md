# C++ / Python golden-vector parity (WS-14 C-2)

**Owner:** WS-05  
**Status:** **C++/Python golden parity verified** — Python 38/38 (`Tests/py/test_golden.py`);
C++ `UEREMCP.Protocol.Golden` (ContentHash, Envelope, Ref, Topo) all **Success** on RE
editor AutomationTests.

`[VERIFIED-RUNTIME: UEREMCP.Protocol.Golden all Success on RE; commit 93bcfa2]`

**Evidence record (2026-07-30):**

| Check | Result |
|---|---|
| Python goldens | 38/38 pass |
| C++ `UEREMCP.Protocol.Golden.ContentHash` | Success |
| C++ `UEREMCP.Protocol.Golden.Envelope` | Success |
| C++ `UEREMCP.Protocol.Golden.Ref` | Success |
| C++ `UEREMCP.Protocol.Golden.Topo` | Success |
| Commit | `93bcfa2` — ContentHash JSON sorted keys (TMap-safe) |
| Golden corpus fingerprint | `sha256:61b087813c3a04831b2367a813e7bef2c050c75f12cdf6dec08666fd7e407308` |

## Contract

| Side | What runs | Must match |
|---|---|---|
| Python | `Tests/py/test_golden.py` via `run_tests.py` | `Tests/golden/**/*.expected.*` |
| C++ | `Private/Tests/UeremcpProtocolGoldenTests.cpp` → `FUeremcp*` | **same** golden files |

Python `ueremcp_protocol/` is a regression harness and golden generator, **not** a
substitute for C++ production code. Regenerating expected outputs:

```bash
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/golden/generate_goldens.py
python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py/run_tests.py
```

If C++ disagrees with a golden, **fix C++** (or deliberately regenerate goldens
and re-verify both sides). Do not "fix" parity by deleting the C++ test.

## Golden suites

| Directory | Exercises |
|---|---|
| `golden/envelope/` | `FUeremcpEnvelope::ParseRequest` + `SerializeResponse` |
| `golden/content_hash/` | `FUeremcpContentHash::HashJsonString` (+ cosmetic stability) |
| `golden/ref/` | `FUeremcpRefResolve::ResolveInPlace` (object + `$op` forms) |
| `golden/topo/` | `FUeremcpDependencyOrder::TopologicalSort` |

## How to run C++ AutomationTests

Requires the UEREMCP plugin built into a UE 5.8 editor target (e.g. the RE
project with this plugin copied/symlinked under `Plugins/UEREMCP`).

Exact command (Windows):

```bat
UnrealEditor-Cmd.exe "$UEREMCP_LEGACY_PROJECT\RE.uproject" ^
  -unattended -NullRHI -nop4 ^
  -ExecCmds="Automation RunTests UEREMCP.Protocol.Golden;Quit"
```

Filter names:

- `UEREMCP.Protocol.Golden.ContentHash`
- `UEREMCP.Protocol.Golden.Envelope`
- `UEREMCP.Protocol.Golden.Ref`
- `UEREMCP.Protocol.Golden.Topo`
- or prefix `UEREMCP.Protocol.Golden`

Golden root discovery (first hit wins):

1. Environment variable `UEREMCP_PROTOCOL_GOLDEN_ROOT`
2. `<ProjectDir>/Plugins/UEREMCP/Source/UeremcpProtocol/Tests/golden`
3. `<ProjectPluginsDir>/UEREMCP/Source/UeremcpProtocol/Tests/golden`

When running from a worktree that is not the project Plugins tree, set:

```bat
set UEREMCP_PROTOCOL_GOLDEN_ROOT=$UEREMCP_ROOT-ws05\Plugins\UEREMCP\Source\UeremcpProtocol\Tests\golden
```

## Parity checklist

- [x] Golden inputs + expected outputs checked in under `Tests/golden/`
- [x] Python `test_golden.py` asserts against goldens
- [x] C++ AutomationTests load the same goldens and call `FUeremcp*`
- [x] C++ AutomationTests observed **PASS** in editor/commandlet (RE, 2026-07-30)

Handoff language: **C++/Python golden parity verified for envelope, content_hash, ref, topo.**
