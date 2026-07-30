# WS-04 → WS-12: modal dialog / editor lockup protection

**Date:** 2026-07-29  
**From:** WS-04  
**Topic:** tool calls blocked by modal UI

## Finding

MCP tool completion is marshalled to the game thread
`[VERIFIED: ModelContextProtocolServer.cpp:851-853]`. Any modal Slate dialog that
blocks the game thread prevents in-editor MCP tools from running — including cancel
and progress handlers.

REAgentTools documents this and ships host-side `UnrealWatchMCP` (Win32 window
enumeration + HTTP health probes) because in-editor tools cannot dismiss modals while
frozen `[VERIFIED: REAgentTools/Optional/UnrealWatchMCP/README.md:9-22]`.

Epic's HTTP server has no modal awareness. A stuck `tools/call` holds the SSE stream
until the tool completes or the client disconnects; there is no engine timeout.

## Recommendation

1. **Wave 1:** Document in `docs/SECURITY.md` / agent guides that agents should use
   `unreal-watch.check_unreal` when MCP times out with the editor apparently up.
2. **Wave 2 (WS-12):** Evaluate porting or wrapping UnrealWatchMCP as an optional
   documented companion — not in-process UEREMCP transport.
3. **Do not** block UEREMCP tool startup on modal detection; that belongs outside the
   editor process.

## Negative finding

No public Epic API exposes "is modal dialog blocking game thread" to MCP plugins
`[UNVERIFIED for exhaustive search — no matches in ModelContextProtocol or
ToolsetRegistry public headers]`.

## Response

**Accepted; routed to WS-12.** Do not put modal dismissal inside UEREMCP transport
(ADR-0002 / ADR-0009). Wave 1: document host-side `unreal-watch` / UnrealWatchMCP
usage when MCP times out with the editor apparently up. Wave 2: WS-12 evaluates
wrapping UnrealWatchMCP as an optional companion. R-11 mitigation updated
accordingly; remains open until the companion path is documented in
`docs/SECURITY.md`.
