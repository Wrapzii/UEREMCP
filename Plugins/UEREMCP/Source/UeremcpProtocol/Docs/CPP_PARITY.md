# C++ / Python golden-vector parity (WS-14 C-2)

**Owner:** WS-05  
**Status:** Python goldens green in CI-less local run; **C++ parity is NOT claimed
until AutomationTests pass** against the same `Tests/golden/` files.

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

## Parity checklist (do not claim done early)

- [x] Golden inputs + expected outputs checked in under `Tests/golden/`
- [x] Python `test_golden.py` asserts against goldens
- [x] C++ AutomationTests load the same goldens and call `FUeremcp*`
- [ ] C++ AutomationTests observed **PASS** in editor/commandlet (pending local UE build)

Until the last box is checked, handoff language is: **Python regression green;
C++ parity pending runtime AutomationTest.**
