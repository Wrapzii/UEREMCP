# Developer setup (local, no editor required)

**Owner:** WS-13. These commands work against a clone / worktree without assuming
Unreal Editor is running. Editor automation is documented but optional.

## Prerequisites

- Python 3.10+ on `PATH`
- Git
- For editor tests only: UE 5.8 + RE project + built UEREMCP plugin (see
  [`Plugins/UEREMCP/README.md`](../../Plugins/UEREMCP/README.md),
  [`tests/README.md`](../../tests/README.md))

## Always-safe local checks

From the repo root of your clone / worktree:

```bash
# Frozen schema contract
python tools/validate_schemas.py

# Guide link + fixture path contract (WS-13)
python docs/guide/check_guide_links.py

# Ownership guard before commit
python tools/check_ownership.py --ws WS-13
```

Optional offline harnesses owned by other workstreams (still no editor):

```bash
# WS-11 unit suite entrypoint (when present)
python tests/run_unit_tests.py

# WS-12 security header/docs contract
python Plugins/UEREMCP/Source/UeremcpSecurity/scripts/test_security_contract.py

# WS-04 transport constraint script
python Plugins/UEREMCP/Source/UeremcpTransport/scripts/test_transport_constraints.py

# WS-14 metrics harness (offline)
python docs/reviews/metrics/test_metrics_harness.py
```

## Editor-dependent (do not require for docs handoff)

Only when the editor, plugin binaries, and RE project are available:

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"
pwsh tests/run_editor_handoff_gates.ps1 -Gate All
```

MCP client config should target loopback only (`http://127.0.0.1:<port>/mcp`) —
[`docs/SECURITY.md`](../SECURITY.md).

## Plugin enablement (human)

1. Build / open the RE project with `Plugins/UEREMCP` enabled.
2. Confirm Epic MCP server auto-start / bind as in SECURITY.
3. `list_toolsets` should show UEREMCP toolsets; `Ping` on Reference must succeed
   before domain work.

Live Coding caveats: see [`docs/research/RB-03-plugin-integration.md`](../research/RB-03-plugin-integration.md).

## Branch / ownership discipline

```text
Branch:  ws-13-<slug>
Commit:  [WS-13] <imperative summary>
Edit:    docs/guide/** only (plus docs/proposals/WS-13-* if needed)
```

Do not commit `Binaries/`, `Intermediate/`, `Saved/`, `__pycache__/`.
