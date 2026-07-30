# WS-01 proposal: ADR-0003 amendment for non-graph asset state

- **From:** WS-01
- **To:** WS-05, WS-10, all domain workstreams
- **Date:** 2026-07-30
- **Status:** amendment draft; frozen schemas unchanged
- **Trigger:** `docs/proposals/ws-10-animation-integration-blockers.md`
- **Proposed ADR:** `docs/adr/ADR-0011-non-graph-asset-state.md`

## Why an amendment is required

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

## Draft amendment

If ADR-0011 is accepted, amend ADR-0003 and the response schema together to add one
typed extension point at `result.asset_state`, plus the same field on each
`result.operations[]` item for batch results. Do not add a domain-specific top-level
field and do not relax any `additionalProperties: false` boundary.

The envelope-level field shape is deliberately narrow:

```json
"asset_state": {
  "type": "object",
  "description": "Complete structured state for one non-graph primary asset. The originating action selects a required domain asset-state schema validated in a second pass."
}
```

This object is not a free-form result bag. Each action that emits it must register
exactly one domain schema:

```text
action -> canonical schema URI

inspect_montage
  -> schemas/domains/animation/inspect_montage.asset-state.schema.json
```

The domain schema owns every field below `asset_state`, must set
`additionalProperties: false`, and should require all fields needed to reconstruct
the advertised complete inspection. The envelope owns only the location, detail
policy, validation dispatch, and compatibility rules.

## Compatibility and versioning

Adding an optional property is backward compatible for new consumers when absent,
but it is not safe to emit to existing strict consumers:
`result.additionalProperties: false` rejects the new field
`[VERIFIED: schemas/envelope/response.schema.json:33-71]`.

The amendment therefore uses these rules:

1. Protocol `1.0` responses never contain `asset_state`.
2. Acceptance of the amendment introduces protocol `1.1`. For matching majors, the
   negotiated minor is the lower of the request minor and the server's highest
   supported minor. A server emits `asset_state` only when that negotiated version is
   at least `1.1` and the request asks for `response_detail: complete`.
3. A `1.1` consumer must accept absence. Absence means the producer did not provide
   typed complete non-graph state; it must not be interpreted as an empty asset.
4. A server handling a `1.0` request retains current counts, references, revision,
   validation, diagnostics, and honest `capability_notes`, but omits the field.
5. The response records the negotiated version. This avoids labeling a response
   `1.0` while emitting a `1.1` field and lets a newer client detect a `1.0` server.
6. Major-version behavior remains unchanged: mismatched majors are rejected
   `[VERIFIED: docs/adr/ADR-0003-request-response-envelope.md:63-64]`.

Current code accepts same-major minor versions and currently reports `1.0`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Private/UeremcpEnvelope.cpp:9,116-133]`.
The acceptance implementation must add minor-version negotiation/emission tests; the
Proposed ADR alone does not authorize changing that code.

## Two-pass response validation

Validation requires the originating request context. The response does not require
an action field, and `understood.action` is optional
`[VERIFIED: schemas/envelope/response.schema.json:20-30]`; validators must not infer
schema selection from summary text, asset class, or optional response metadata.

The accepted implementation must perform:

1. **Envelope pass.** Validate the response against the schema for the negotiated
   protocol version. This checks the closed outer shape and that each `asset_state`,
   when present, is an object.
2. **Action/domain pass.** Using the originating request action, enforce detail and
   status policy, resolve the registered asset-state schema, and validate the complete
   object against it. Reject `asset_state` below `response_detail: complete`. For an
   action that advertises complete non-graph inspection, a successful complete
   response must include it. Unknown action, missing registration, schema-load
   failure, or domain validation failure makes response validation fail; no untyped
   pass-through is allowed. Failure/rejection/timeout responses may omit state; an
   operation that ran but cannot provide required complete state remains
   `partially_completed` with the reason.

`FUeremcpResponse` should gain a typed JSON-object member rather than route the field
through `ExtraFields`; serialization and validation then have one explicit code path.
The exact registry API belongs to WS-05, but its observable contract is action plus
canonical schema URI, duplicate registration rejection, and deterministic lookup.

## Batch behavior

`execute_plan` is not itself a domain action, so one top-level
`result.asset_state` cannot select a domain schema. For batch responses:

- top-level `result.asset_state` is omitted;
- each `result.operations[]` item may carry its own optional `asset_state`;
- `operations[].action` is required whenever that item carries `asset_state` and
  selects the domain schema;
- the request's one top-level `response_detail` applies to every operation because
  batch operation specifications have no per-operation options
  `[VERIFIED: schemas/batch/plan.schema.json:45-103]`;
- at `complete`, every successful operation that advertises complete non-graph state
  includes its state, even if several operation payloads are large;
- skipped, rejected, failed, rolled-back, or still-running operations omit state and
  retain an explanatory summary or `skipped_reason`;
- the validator matches response operation IDs and actions to the originating plan
  before domain validation. Duplicate IDs, action mismatches, or state on an unknown
  operation fail validation.

This preserves one semantic batch call and avoids a follow-up inspect loop. A separate
top-level map keyed by operation ID is rejected because it duplicates
`result.operations[]` identity and separates state from operation status.

## Animation montage population

After acceptance, `inspect_montage` at `response_detail: complete` populates
`result.asset_state` with the service's structured object: identity, class, duration,
blend/sync settings, skeleton, every slot and segment, every section, every real
notify/state entry, `content_hash`, and `revision`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpAnimation/Private/UeremcpAnimationService.cpp:50-161]`.

Its domain schema is
`schemas/domains/animation/inspect_montage.asset-state.schema.json`. WS-10 owns that
schema and must make its required fields and closed nested shapes match the emitted
object. `minimal`, `summary`, and `diagnostic` omit the object. Protocol `1.0` also
omits it regardless of detail. Dependencies remain in `result.dependencies`; they
are not removed merely because paths also appear inside complete state.

Until acceptance and implementation, WS-10 keeps withholding the structured object
and reports `partially_completed` when complete state was requested.

## Acceptance package

ADR-0011 should move from Proposed to Accepted only when one coordinated change
contains:

1. ADR-0011 status change and an explicit amendment note in ADR-0003;
2. protocol `1.1` envelope schema and version negotiation behavior;
3. typed response and batch-operation serializer support;
4. action-to-asset-state schema registration and two-pass validation;
5. WS-10's closed montage asset-state schema and complete-detail population;
6. unit/golden tests for absence compatibility, `1.0` emission rejection, `1.1`
   inclusion, lower-detail omission, unknown registration, malformed domain state,
   and mixed-action batches;
7. all schema examples and `python tools/validate_schemas.py` passing.

No experimental schema stub is added by this proposal. WS-10 may draft its owned
domain schema now, but neither WS-10 nor WS-05 should emit `asset_state` into the
frozen envelope until the acceptance package lands.
