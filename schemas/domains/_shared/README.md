# Shared domain schema fragments

**Owner:** WS-05.

Cross-domain pieces that are not the envelope, not the graph schema, and not a
single domain's `specification`. Domain workstreams `$ref` these rather than
redefining them.

| File | Purpose |
|---|---|
| `result_ref.schema.json` | Provisional `{"$ref":"<op>.<path>"}` substitution object used inside batch specifications. **Grammar not final** — see `docs/proposals/ws-05-batch-grammar-blocked.md`. |

Content-hash rules live with the protocol module:
`Plugins/UEREMCP/Source/UeremcpProtocol/Docs/CONTENT_HASH.md`.
