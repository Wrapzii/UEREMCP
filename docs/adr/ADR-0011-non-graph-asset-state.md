# ADR-0011: Typed non-graph asset state in responses

- **Status:** Proposed
- **Date:** 2026-07-30
- **Owner:** WS-01
- **Unblocks:** WS-10 complete montage inspection; domain-complete non-graph reads
- **Depends on:** ADR-0003, ADR-0006,
  `docs/proposals/ws-01-non-graph-asset-state.md`

## Context

ADR-0003 freezes the response envelope and permits domain extension only in request
`specification` `[VERIFIED: docs/adr/ADR-0003-request-response-envelope.md:21-39]`.
The response root and `result` reject unknown fields, and the only complete structured
state slot is `diagnostics.graphs`, whose items must use the graph schema
`[VERIFIED: schemas/envelope/response.schema.json:8,33-71,109-140]`.

An animation montage inspection produces structured asset state—slots, segments,
sections, notifies, dependencies, and revision—but that state is not a graph
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpAnimation/Private/UeremcpAnimationService.cpp:50-161]`.
The current contract therefore cannot return it without loss or schema violation.
The complete-state objective favors one typed response over a follow-up inspect loop
`[VERIFIED: docs/WHY.md:42-47,97-115]`.

This ADR is an amendment proposal, not authorization to edit ADR-0003's frozen schema.

## Proposed decision

If accepted, amend ADR-0003 with an optional `asset_state` object under `result` and
under each `result.operations[]` item. The originating action selects a required,
closed domain schema in a second validation pass. The field is emitted only when the
negotiated protocol is at least `1.1` and `response_detail` is `complete`.

### Field contract

The envelope schema constrains `asset_state` to an object. It does not define domain
fields and does not allow arbitrary data. Every emitting action registers one
canonical schema at:

```text
schemas/domains/<domain>/<action>.asset-state.schema.json
```

That domain schema must close all object shapes with `additionalProperties: false`
and require the fields that constitute the action's advertised complete state.
`result.asset_state` describes the response's one non-graph primary asset; it does
not replace asset references, dependencies, validation evidence, revision, or
diagnostics.

### Compatibility and versioning

Default absence is backward compatible: consumers must accept a response with no
`asset_state`, and absence never means an empty asset.

Emission is version-gated because existing strict `1.0` consumers reject unknown
result properties `[VERIFIED: schemas/envelope/response.schema.json:33-71]`:

- `1.0` responses omit `asset_state`;
- acceptance introduces `1.1`;
- for matching majors, the negotiated minor is the lower of the request minor and the
  server's highest supported minor;
- only a negotiated version of at least `1.1` may receive the field;
- the response reports the negotiated version;
- major mismatch rejection remains unchanged.

The current implementation accepts same-major versions and reports protocol `1.0`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Private/UeremcpEnvelope.cpp:9,116-133]`.
Acceptance therefore requires explicit minor negotiation and regression tests rather
than relying on same-major parsing alone.

### Two-pass validation

Validation receives the originating request context:

1. Validate the complete response against the negotiated envelope schema.
2. For each action, enforce detail/status policy, then resolve its registered schema
   and validate each present `asset_state` against it. State below `complete` is
   rejected; successful complete responses for actions advertising complete
   non-graph inspection require state.

The response's optional `understood.action` is not the authority for schema dispatch
`[VERIFIED: schemas/envelope/response.schema.json:20-30]`. Missing registration,
duplicate registration, unknown action, schema-load failure, malformed domain state,
or action mismatch fails validation. Failure, rejection, timeout, and rolled-back
responses may omit state. If an operation ran but required complete state cannot be
returned, it reports `partially_completed` and the reason.

### Batch behavior

For `execute_plan`, top-level `result.asset_state` is omitted because `execute_plan`
does not identify one domain schema. Each `result.operations[]` item may instead carry
`asset_state`; its required `id` and conditionally required `action` are matched to
the originating plan before action-selected validation. `action` is required whenever
that item carries `asset_state`.

The request's top-level `response_detail` applies to all operations because batch
operations do not define per-operation options
`[VERIFIED: schemas/batch/plan.schema.json:45-103]`. At `complete`, each successful
operation advertising non-graph complete state includes its payload. Skipped, failed,
rejected, rolled-back, or running operations omit it and retain their explanatory
status fields. Large mixed-action results remain inline to preserve one semantic
operation; resource transport does not replace the authoritative typed result.

### Animation montage

After acceptance, `inspect_montage` populates `result.asset_state` at complete detail
from its existing service inspection. WS-10 defines the exact closed schema in
`schemas/domains/animation/inspect_montage.asset-state.schema.json`. Lower detail
levels and protocol `1.0` omit state while retaining compact counts, dependencies,
revision, validation evidence, metrics, and limitations.

Until the coordinated amendment lands, WS-10 must not emit the field into the frozen
envelope and must continue honest partial reporting when complete state is requested.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Put montage state in `summary`, diagnostics strings, or `capability_notes` | Lossy and not machine-validatable. |
| Encode non-graph assets as synthetic graphs | Misrepresents the domain and violates the graph-family contract. |
| Relax `result.additionalProperties` | Creates an untyped response bag and defeats cross-domain validation. |
| Add one top-level field per domain | Repeatedly changes the frozen envelope and fragments consumers. |
| Put batch state in a top-level map keyed by operation ID | Duplicates operation identity and separates state from its action/status. |
| Return only an MCP resource link | Requires another retrieval and makes the inline result non-authoritative. |
| Emit the optional field under protocol `1.0` | Existing strict consumers reject unknown result properties. |

## Consequences

**Enables:** complete non-graph inspection in one response, beginning with montage
state; action-owned schemas; deterministic validation for single and mixed-action
batches.

**Costs:** protocol minor-version negotiation, a response-schema registry, a second
validation pass, typed serializer changes, and potentially large complete batch
responses.

**Locks in:** one shared non-graph extension point and the
`<action>.asset-state.schema.json` naming convention. Reversing it later would touch
every emitting domain and consumer.

## Open questions

- Which WS-05 registry type owns action-to-schema registration and schema caching?
- Whether future oversized responses need a separate, versioned resource-link
  amendment while preserving inline authoritative semantics.
- Whether any action legitimately has more than one non-graph primary asset; such a
  need requires a new proposal rather than overloading this field.

## Verification

Acceptance requires one coordinated change with:

1. ADR-0003 amendment text and protocol `1.1` envelope schemas;
2. typed single and batch serialization;
3. two-pass action-selected response validation;
4. a closed WS-10 montage asset-state schema and complete-detail population;
5. tests covering absent state, `1.0` emission rejection, `1.1` inclusion,
   lower-detail rejection, unknown schema registration, malformed domain state,
   action/operation mismatch, and mixed-action batches;
6. `python tools/validate_schemas.py` passing.
