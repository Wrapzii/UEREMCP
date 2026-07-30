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

## Response / status (WS-01)

**Partial — 2026-07-30.**

WS-05 landed `c8007e3` (`SortJsonKeys` + pin sort `direction|name`); merged to orch
`6a88cb2`. Python still **38/38**. Synced source into RE junction (ws03), rebuilt
`UeremcpProtocol` with `-NoHotReloadFromIDE`, re-ran:

`UEREMCP.Protocol.Golden` → Envelope/Ref/Topo **Success**; ContentHash **still Fail**.

| Stage | Hash |
|---|---|
| Golden / Python | `sha256:61b087813c3a04831b2367a813e7bef2c050c75f12cdf6dec08666fd7e407308` |
| Pre-fix C++ | `sha256:d21735…` |
| Post-`SortJsonKeys` C++ | `sha256:2637e05f6a390ef04cf74920ddb2a41da61fd841c841095b22860c849e7aa036` |

### Remaining root cause (WS-05)

`SortJsonKeys` inserts keys in sorted order into `FJsonObject`, but
`FJsonSerializer::Serialize` iterates `FJsonObject::Values` (`TMap`), which is
**not insertion-ordered**. Python’s `json.dumps(..., sort_keys=True)` sorts at
emit time. CONTENT_HASH.md step 7 requires lexicographic keys in the **byte
string**.

**Fix:** replace `WriteCanonical`’s `FJsonSerializer::Serialize` with a recursive
writer that emits object keys via an explicitly sorted key list (same as Python),
or otherwise guarantee sorted emit independent of `TMap` iteration. Then rebuild
Protocol (Live Coding off) and re-run `UEREMCP.Protocol.Golden.ContentHash`.
