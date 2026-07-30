# Shared domain schema fragments

**Owner:** WS-05.

Cross-domain pieces that are not the envelope, not the graph schema, and not a
single domain's `specification`. Domain workstreams `$ref` these rather than
redefining them.

| File | Purpose |
|---|---|
| `result_ref.schema.json` | Final batch result `$ref`: object form `{"$ref":"<op>.<path>"}` **and** dollar-string `"$<op>"` (REAgentTools). See `docs/proposals/ws-05-batch-ref-grammar.md`. |

Content-hash / job / batch-ref docs live with the protocol module under
`Plugins/UEREMCP/Source/UeremcpProtocol/Docs/`.
