# WS-14 proposal: Rewire Ping/Echo to `FUeremcpEnvelope`

- **From:** WS-14
- **To:** WS-03 (+ WS-05 for version constant)
- **Date:** 2026-07-30
- **Status:** Open

## Problem

Wave 1b closed C-2 for **Protocol module** golden tests (`FUeremcpEnvelope`, etc.), but
the **production MCP tools** still use `UeremcpMinimalEnvelope.h`:

- `UeremcpReferenceToolset.cpp` lines 29–88
- `UeremcpCore.Build.cs` lines 30–32 (no `UeremcpProtocol` dependency)

Agents calling `UeremcpCore.UeremcpReferenceToolset.Echo` therefore do not exercise the
code path covered by `UEREMCP.Protocol.Golden.*` automation tests.

Additional drift:

- `MinimalEnvelope` emits `protocol_version` `"1.0.0"`; schema and `FUeremcpEnvelope`
  use `"1.0"` (`schemas/common/defs.schema.json`, `UeremcpEnvelope.cpp:9`).
- Echo automation test sends `"1.0.0"` (`UeremcpReferenceToolsetTests.cpp:113`).

## Required action

1. Add `UeremcpProtocol` to `UeremcpCore` private dependencies.
2. Replace `MinimalEnvelope` calls with `FUeremcpEnvelope::ParseRequest` /
   `SerializeResponse` (or thin wrappers).
3. Delete or reduce `UeremcpMinimalEnvelope.h` to test-only helpers.
4. Update ReferenceToolset automation tests to use schema-valid `"1.0"` requests and
   assert rejection of malformed envelopes per golden cases.
5. Re-capture `describe_toolset` dump if Echo parameter schema changes.

## Acceptance

MCP Echo path and `UEREMCP.Protocol.Golden.Envelope` share one implementation; no
duplicate envelope parsers in shipping Core.

## Response (WS-01)

**Date:** 2026-07-30  
**Decision:** Routed to **WS-03** (owner: `UeremcpCore` / ReferenceToolset). WS-01 will not rewire Core Echo in this integration; track under Wave 2 host hardening (M-1).
