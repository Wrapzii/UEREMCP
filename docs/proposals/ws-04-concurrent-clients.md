# WS-04 → WS-01: concurrent MCP clients and ADR-0006

**Date:** 2026-07-29  
**From:** WS-04  
**Topic:** multi-agent concurrency on one editor

## Finding

Each `initialize` creates a new `FModelContextProtocolSession` with a unique
`Mcp-Session-Id` `[VERIFIED: ModelContextProtocolServer.cpp:664-671]`. Sessions are
stored in a server-side array; there is no documented single-client limit
`[VERIFIED: ModelContextProtocolServer.h:66]`.

Multiple agents can connect concurrently **at the transport layer**. Tool execution
still funnels through one editor process and largely the game thread
`[VERIFIED: ModelContextProtocolServer.cpp:851-853, IModelContextProtocolTool.h:30-31]`.

`FToolsetRegistryToolAdapter` does not serialize or queue calls; parallel
`tools/call` from different sessions can interleave arbitrary editor mutations.

## Recommendation

ADR-0006 idempotency keys protect individual operations but do not serialize the
editor. For swarm use:

1. Document that concurrent writers are **best-effort** unless WS-12 adds a project
   lock or queue.
2. Prefer one writer agent per project; readers may be concurrent if tools are
   read-only.
3. Consider a UEREMCP-side optional job queue (single active mutator) in Wave 2 —
   not Epic's transport problem.

## No ADR contradiction

ADR-0002/0006 do not promise parallel safe writes; this finding tightens operational
guidance for multi-agent setups.

## Response

**Accepted as operational guidance.** Document for swarm use:

1. Concurrent MCP sessions are transport-supported; concurrent **writers** are
   best-effort only.
2. Prefer one writer agent per project; read-only tools may run concurrently.
3. Optional single-mutator job queue is **Wave 2 / WS-12**, not a transport rewrite.

ADR-0006 remains the conflict model for individual ops. R-12 stays open until WS-12
lands project-level serialization if needed. No frozen ADR contradicted.
