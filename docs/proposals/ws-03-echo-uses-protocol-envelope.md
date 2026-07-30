# WS-03 response: Ping/Echo now route through `FUeremcpEnvelope`

- **From:** WS-03
- **To:** WS-14 (closes `ws-14-ws03-echo-protocol-rewire.md`)
- **Date:** 2026-07-30
- **Status:** Implemented

## What changed

1. `UeremcpCore.Build.cs` — added private dependency on `UeremcpProtocol`.
2. `UeremcpReferenceToolset.cpp` — Ping/Echo now use
   `FUeremcpEnvelope::ParseRequest`, `SerializeResponse`, and `MakeRejection`.
3. Deleted `Private/UeremcpMinimalEnvelope.h` (duplicate parser removed from shipping Core).
4. `UeremcpReferenceToolsetTests.cpp` — requests use schema-valid `protocol_version: "1.0"`
   and action `reference_echo` (matches `^[a-z][a-z0-9_]*$`).

## Version-string migration

| Before (`MinimalEnvelope`) | After (`FUeremcpEnvelope`) |
|---|---|
| `"1.0.0"` | `"1.0"` (matches `schemas/common/defs.schema.json`) |
| `understood.target` | `understood.resolved_target` |

Agents or fixtures still sending `"1.0.0"` will be **rejected** by
`IsProtocolCompatible` (pattern requires exactly `MAJOR.MINOR`). Update callers to `"1.0"`.

## Acceptance

Production MCP Ping/Echo and `UEREMCP.Protocol.Golden.Envelope` now share one envelope
implementation. No duplicate parsers remain in shipping `UeremcpCore`.
