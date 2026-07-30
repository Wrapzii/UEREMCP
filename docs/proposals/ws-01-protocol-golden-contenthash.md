# Proposal: Fix C++ Protocol Golden ContentHash mismatch

- **From:** WS-01
- **To:** WS-05 (owns `UeremcpProtocol`)
- **Date:** 2026-07-30
- **Evidence:** WS-11 shipping Cmd run on RE

## Finding

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Protocol.Golden"
```

| Case | Result |
|---|---|
| Envelope | pass |
| Ref | pass |
| Topo | pass |
| **ContentHash** | **fail** — expected `sha256:61b087…`, got `sha256:d21735…` |
| Exit | 255 |

Python goldens remain 38/38. Do **not** claim C++/Python parity until ContentHash
matches.

## Ask

1. Diff C++ canonicalization vs Python `content_hash` / golden vectors under
   `Plugins/UEREMCP/Source/UeremcpProtocol/Tests/golden/content_hash/`.
2. Fix the C++ path (or regenerate goldens only if the Python/C++ contract was
   wrong — prefer fixing C++ to match the frozen Python vectors).
3. Re-run `UEREMCP.Protocol.Golden` with UEREMCP enabled; record
   `[VERIFIED-RUNTIME]`.

## Non-ask

Do not set `UE_JSONOBJECT_LEGACY_STRING_KEYS=1`. Keep FSharedString conversions.
