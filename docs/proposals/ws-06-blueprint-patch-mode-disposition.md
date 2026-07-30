# WS-06: Blueprint patch-mode disposition

- **From:** WS-06
- **Date:** 2026-07-30
- **Status:** Formally rejected for implementation pending a typed contract

## Decision

`blueprints.submit_graph` continues to reject `mode=patch` without loading or
mutating the target. The response now carries stable diagnostic code
`blueprint.patch_contract_undefined`, reports zero internal operations, and directs
callers to the supported complete-graph `mode=replace` path.

This is a contract insufficiency, not evidence that semantic patching is incompatible
with ADR-0004. Implementation must wait until the Blueprint domain schema defines
typed operations precisely enough to validate intent and verify the result.

## Accepted-design evidence

1. ADR-0004 accepts `patch` as “submit a semantic diff,” but does not define an
   operation vocabulary, required operands, ordering, failure atomicity, or
   post-write comparison rules (`docs/adr/ADR-0004-graph-representation.md:58-60`).
2. ADR-0004 also locks the normal graph exchange to complete structured JSON and
   states that patches reference cross-retrieval `semantic_id`, not retrieval-local
   `node_id` (`docs/adr/ADR-0004-graph-representation.md:47-60`).
3. ADR-0006 requires `patch` to reject when the target is absent and requires stale
   `expected_revision` handling, but does not define patch application semantics
   (`docs/adr/ADR-0006-idempotency-revisions.md:31-41,52-71`).
4. The current WS-06 schema requires only `op`; every operation otherwise permits
   arbitrary properties through `additionalProperties: true`
   (`schemas/domains/blueprints/submit_graph.schema.json:26-46`).
5. The two existing WS-06 documents explicitly label the operation shape
   “draft,” “proposal only,” and “not implemented”
   (`docs/proposals/ws-06-patch-mode-and-impl-plan.md:48-65`;
   `docs/proposals/ws-06-submit-graph-patch-mode.md:1-16`).
6. Those drafts disagree on operand names (`pin_defaults` versus `defaults`, and
   `from`/`to` versus `from_semantic_id`/`to_semantic_id`), demonstrating that the
   permissive schema is not an implementation contract.

## Audit disposition

No new node or pin primitive is added. WS-06 continues to compose Epic
`BlueprintTools` internally and expose only the semantic `read_graph` /
`submit_graph` surface. The Blueprint audit was already folded into the WS-02 matrix
(`docs/proposals/ws-06-audit-blueprint-rows.md`, response records `b15ee88` /
orchestrator `e61293e`).

Epic already supplies graph DSL, node, pin, connection, compile, and batched script
operations `[VERIFIED: docs/GROUNDED_FACTS.md:345-356]`. REAgentTools has no
Blueprint graph authoring and defers to Epic `[VERIFIED:
docs/GROUNDED_FACTS.md:449-460]`. A second primitive layer would duplicate existing
capability and violate the audit-first rule.

## Required contract before implementation

A future schema revision needs, at minimum:

- a closed schema for each operation (`oneOf` with `additionalProperties: false`);
- required operands and value shapes for every operation;
- `semantic_id` resolution and missing/ambiguous-target behavior;
- operation ordering, duplicate-op, and intra-patch dependency rules;
- relationship between `patch.base_revision` and envelope `expected_revision`;
- atomic failure and no-op rules;
- a deterministic expected-after state or hash used by the mandatory re-read.

Until those decisions are accepted, implementing the draft would invent protocol
semantics and risk silently mutating the wrong Blueprint node.

## Implemented hardening

- stale `read_graph` text claiming replace was unimplemented was removed;
- patch has a dedicated rejection branch and stable machine-readable diagnostic;
- rejection explicitly records skipped conflict, mutation, compile, and re-read
  checks;
- submit validation now emits `validation.reread_after_write` from actual performed
  checks, so a validated write claim has direct evidence instead of only a string in
  `checks_performed`;
- native tests cover patch rejection/no mutation, conflict, repeated no-op replace,
  and re-read evidence policy without weakening replace.

## Remaining limitation

Patch remains unavailable. Complete-graph replace remains scratch-path scoped, and
its existing fidelity limitations continue to be reported. This disposition makes
no POC-A claim beyond the already scoped CRT.
