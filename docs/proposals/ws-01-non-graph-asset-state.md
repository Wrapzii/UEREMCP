# WS-01 decision: non-graph asset state needs an envelope amendment

- **From:** WS-01
- **To:** WS-05, WS-10, all domain workstreams
- **Date:** 2026-07-30
- **Status:** proposal only; frozen schemas unchanged
- **Trigger:** `docs/proposals/ws-10-animation-integration-blockers.md`

## Decision

Choose option B: the current frozen contract has no conformant extension point for
structured non-graph response state.

ADR-0003 makes `specification` the only domain-extensible field, but
`specification` is request input. It cannot carry the result of `inspect_montage`
`[VERIFIED: docs/adr/ADR-0003-request-response-envelope.md:21-39]`.

The response root and `result` reject unknown properties, while
`diagnostics.graphs` accepts only `graph.schema.json`
`[VERIFIED: schemas/envelope/response.schema.json:8,33-71,109-140]`.
ADR-0004 applies that graph shape to graph families; an AnimMontage's slots,
segments, sections, and notifies are asset state, not a graph
`[VERIFIED: docs/adr/ADR-0004-graph-representation.md:25-45]`.

An action-specific response schema cannot repair this by composing with the current
envelope: `result.additionalProperties: false` still rejects `asset_state`. Encoding
the state in `summary`, diagnostic strings, `capability_notes`, or a synthetic graph
would be lossy or contract-invalid. A resource link is also only a complement:
ADR-0009 explicitly keeps `result` authoritative
`[VERIFIED: docs/adr/ADR-0009-long-running-jobs.md:60-63]`.

Therefore no envelope, graph, common, or animation schema is changed in this commit.

## Recommended extension point

Amend ADR-0003 in a deliberate protocol change to add one optional
`result.asset_state` object. Do not add a domain-specific top-level field and do not
relax `result.additionalProperties`.

Validation should mirror request `specification`:

1. validate the unchanged outer response envelope;
2. when `result.asset_state` is present, select an action-owned asset-state schema by
   `action`;
3. validate the object against
   `schemas/domains/<domain>/<action>.asset-state.schema.json`;
4. reject an unregistered action/schema pair rather than accepting a free-form bag.

`asset_state` is present only for `options.response_detail: complete`. `minimal`,
`summary`, and `diagnostic` retain compact identity, counts, validation evidence, and
revision. MCP resources may additionally carry oversized complete bodies, but the
typed inline state remains the normative result unless a later ADR explicitly changes
that rule.

The smallest eventual envelope addition is:

```json
"asset_state": {
  "type": "object",
  "description": "Complete structured state for the non-graph primary asset. The action selects a required domain asset-state schema validated in a second pass."
}
```

This is intentionally a recommendation, not a schema edit. Before landing it, WS-01
and WS-05 must define protocol compatibility, serializer support, action-to-schema
registration, and tests. Because ADR-0003 freezes the response shape, those changes
must land together with an ADR amendment rather than piecemeal.

## Required follow-ups

### WS-10

1. Keep `inspect_montage` at `partially_completed` while structured state is withheld.
2. Draft
   `schemas/domains/animation/inspect_montage.asset-state.schema.json` from the
   service's emitted object, including all required fields and
   `additionalProperties: false`; do not edit the envelope.
3. After the ADR/protocol amendment lands, populate `result.asset_state` only at
   `response_detail: complete`; retain counts, dependencies, revision, validation,
   metrics, and honest `capability_notes` at lower detail levels.
4. Add contract tests for complete-state inclusion, lower-detail omission,
   action-selected schema validation, and rejection of malformed state.
5. Run the already-assigned editor lane before promoting beyond `partial`: build,
   `UEREMCP.Animation.*`, toolset discovery, and a real montage inspection.

### WS-07

No response-contract change is requested from WS-07. Keep `inspect_system` and
`create_niagara_effect` at `partial`: they are `/Game/__UeremcpTests/` Wave-2 probes,
and create still lacks renderer/material and runtime-smoke validation. Update the
catalog only after those checks run; do not infer promotion from registration or
tool-call completion.

### WS-05 / WS-01

Specify and test the two-pass response validator, `FUeremcpResponse` serialization,
backward compatibility/versioning, and batch-operation behavior before touching
`schemas/envelope/response.schema.json`. Re-run all schema examples and protocol
goldens when the amendment is authorized.
