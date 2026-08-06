# Prepared-action discovery policy

Status: implementation handoff for WS-13 documentation integration

Normal UEREMCP discovery should use the capability layer on
`UeremcpCore.UUeremcpReferenceToolset`:

1. `SearchCapabilities` or `ResolveAndPrepare` reads the live registry and
   returns a bounded result with a registry hash.
2. Retrieve one `GetCapabilityContract` response when the compact action contract
   is insufficient. Use `detail=full` only when the complete input schema is
   necessary.
3. Execute only a returned `action_id` through `ExecutePreparedAction`.

Prepared actions expire, carry a registry and contract hash, bind resource paths
and expected revisions, and preserve the original risk ceiling. The endpoint
rejects arbitrary tool names and arguments. Mutation actions require confirmation;
dry runs do not call the underlying ToolsetRegistry tool. Results are structured
objects and include bounded continuations.

Primitive tools remain available for advanced or unsupported cases. Agents should
not repeatedly call `list_toolsets` / `describe_toolset` for normal workflows or
manually stage full JSON responses unless diagnosing a server problem.

The implementation currently uses a generic package timestamp revision for
cross-domain preparation. Domain-specific structural revision checks remain
authoritative. See `Saved/ForgeMeta/MCPDiagnostics/prepared_action_layer_design.md`.
