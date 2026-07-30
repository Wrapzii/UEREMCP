# WS-04 → WS-01: MCP resources for large graph payloads

**Date:** 2026-07-29  
**From:** WS-04  
**Topic:** ADR-0004 payload-size mitigation via MCP resources

## Finding

Epic's server implements `resources/list` and `resources/read` and exposes
`IModelContextProtocolResourceProvider` for registration
`[VERIFIED: ModelContextProtocolServer.cpp:35-36, IModelContextProtocolResourceProvider.h:16]`.
Initialize advertises `Capabilities.Resources`
`[VERIFIED: ModelContextProtocolServer.cpp:677-680]`.

Tool results also support `ResourceLink` and `EmbeddedResource` result types
`[VERIFIED: ModelContextProtocolToolResults.h — EModelContextProtocolToolResultType]`.

## Recommendation

For `response_detail: complete` graph payloads that exceed comfortable inline tool
result size, UEREMCP should register a resource provider that serves versioned graph
JSON by stable URI (e.g. `ueremcp://graph/<asset_path>?rev=<hash>`), returning a
`resource_link` in the summary response and full body via `resources/read`.

This does **not** replace the envelope `result` block; it complements it for bulk
data while keeping default `summary` responses small (ADR-0003 rule 1).

## WS-01 action

Confirm in ADR-0004 addendum or ADR-0009 whether resource URIs are normative for
`complete` detail, and assign implementation to the domain workstream that owns
the graph serializer.
