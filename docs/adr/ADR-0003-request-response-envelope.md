# ADR-0003: Versioned JSON request/response envelope

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** every workstream that defines a tool
- **Depends on:** ADR-0002

## Context

Master prompt §27.9 is explicit: *"Define common JSON schemas before domain agents
independently invent incompatible formats."* This ADR exists to be written before the
swarm starts, not after.

Epic's execution contract is already `FString JsonInput` → `TFuture<TValueOrError<FString, FString>>`
`[VERIFIED: $TR/.../Public/ToolsetRegistry/Toolset.h]`, so a JSON envelope is the
native shape rather than an imposition.

## Decision

Every agent-facing UEREMCP tool accepts exactly one JSON object conforming to
`schemas/envelope/request.schema.json` and returns exactly one JSON object
conforming to `schemas/envelope/response.schema.json`.

Frozen envelope invariants — domain workstreams extend `specification`, never the
envelope:

**Request:** `protocol_version` (required), `request_id`, `action`, `project`,
`target`, `mode`, `specification`, `options`, `expected_revision`, `idempotency_key`.

- `mode` ∈ `create` | `create_or_update` | `replace` | `patch` |
  `rebuild_from_specification` | `repair` | `delete`
- `options` ∈ `dry_run`, `atomic`, `rollback_on_failure`, `compile`, `validate`,
  `save`, `response_detail`, `timeout_ms`, `on_revision_conflict`
- `response_detail` ∈ `minimal` | `summary` | `diagnostic` | `complete`, default
  `summary`
- **`specification` is the only domain-extensible field.** Its shape is selected by
  `action` and defined by the owning domain workstream in
  `schemas/domains/<domain>/`.

**Response:** `protocol_version`, `request_id`, `status`, `summary`, `result`,
`validation`, `changes`, `diagnostics`, `revision`, `rollback`, `metrics`.

- `status` ∈ `created_and_validated` | `modified_and_validated` |
  `created_with_warnings` | `no_change_required` | `failed_validation` |
  `rolled_back` | `partially_completed` | `rejected` | `error`
- `result` carries `primary_asset`, `created_assets`, `modified_assets`,
  `deleted_assets`, `reused_assets`, `dependencies`
- `metrics` carries `timing_ms`, `internal_operations`, `mcp_round_trips` — see
  ADR rationale below

Four rules that are not negotiable:

1. **`response_detail: summary` must never return raw Unreal object dumps.** Bulk
   graph payloads appear only at `complete`. The default response is small enough to
   be worth reading in full.
2. **`status` reflects verified reality, not tool-call completion** (`AGENTS.md`
   rule 6). A tool that did not verify returns `partially_completed` with the reason,
   never a `*_validated` status.
3. **`metrics.mcp_round_trips` and `metrics.internal_operations` are mandatory on
   every response.** Round-trip reduction is the project's headline success metric
   (master prompt §19, §25); a metric that is optional does not get measured.
4. **`protocol_version` is checked on every request.** Mismatched major version is
   `rejected` with an explanatory `summary`, never best-effort parsed.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| Typed `UFUNCTION` parameters per tool, letting UHT generate a bespoke schema each | Loses one uniform envelope; `options`, `dry_run`, revisions, and metrics would be re-declared per tool and drift. Rejected for agent-facing tools; fine for internal primitives. |
| Envelope-less, action-specific top-level JSON | Same drift problem, plus no place for cross-cutting concerns. |
| Envelope with a free-form `data` bag | Unschemable, unvalidatable, and invites exactly the incompatible-format problem §27.9 warns about. |
| Defer envelope design until domains report in | Guarantees fourteen incompatible formats. This ADR exists specifically to prevent that. |

## Consequences

**Enables:** one validator, one versioning story, uniform dry-run/rollback/detail
semantics for free in every domain, and comparable metrics across all operations.

**Costs:** the envelope is ceremony for genuinely trivial operations, and adding a
cross-cutting field later means touching every tool. Accepted — cross-cutting fields
should be rare, and the envelope is deliberately generous up front to make additions
rarer.

**Locks in:** the outer shape. `specification` remains free for domains, so this is
cheap to live with and expensive to change.

## Open questions

- ~~How does an `AICallable` `UFUNCTION` taking one `FString` present its schema to the
  agent?~~ **Closed by RB-03 q6** `[VERIFIED-RUNTIME: RegisterAndCaptureSchema]`:
  UHT emits only `{"type":"string","description":"..."}` for the envelope
  parameter — no nested envelope object schema at the MCP tool boundary. Agents
  must get field guidance from `@param` / tool description text, `describe_action`,
  or a future hybrid `USTRUCT` + JSON `specification` (RB-03 q7). Residual
  discoverability risk remains **R-04**.
- ~~Whether `expected_revision` belongs in the envelope or per-operation inside batches~~
  **Closed:** both — different scopes
  (`docs/proposals/ws-05-expected-revision-scope.md`).

## Verification

`python tools/validate_schemas.py` passes, and every example in
`schemas/examples/` validates against the frozen schemas.
