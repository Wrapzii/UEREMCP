# Schemas

**Owner:** WS-01, except `batch/` and `domains/_shared/` (WS-05) and
`domains/<domain>/` (the owning domain workstream).

**These are frozen.** Master prompt §27.9: define common JSON schemas *before* domain
agents independently invent incompatible formats. That is the entire purpose of this
directory, and it only works if nobody forks it.

```bash
python tools/validate_schemas.py
```

## Files

| Path | Contents | ADR | Owner |
|---|---|---|---|
| `common/defs.schema.json` | Shared primitives: paths, hashes, statuses, diagnostics, metrics | 0003 | WS-01 |
| `envelope/request.schema.json` | The one request object every tool accepts | 0003 | WS-01 |
| `envelope/response.schema.json` | The one response object every tool returns | 0003 | WS-01 |
| `graph/graph.schema.json` | Complete graph representation, all families | 0004 | WS-01 |
| `batch/plan.schema.json` | `execute_plan` payload — dependencies, `$ref`, transactions | 0003 | WS-05 |
| `template-library/template.schema.json` | Reusable construction patterns | 0008 (pending) | WS-01 |
| `domains/<domain>/` | Per-action `specification` schemas | — | domain WS |
| `examples/` | Standalone examples, each naming its schema via `x-schema` | — | WS-01 |

## The one rule

**Extend `specification`. Never the envelope.**

`specification` is the only domain-extensible field in a request. Its shape is selected
by `action` and defined by the owning workstream under `domains/<domain>/`. Everything
else — `mode`, `options`, `expected_revision`, `idempotency_key`, and the whole response
shape — is shared, and adding a field there means touching every tool in the project.

If you genuinely need a cross-cutting field, that is a proposal to WS-01, not an edit.

## Adding a domain schema

1. `domains/<domain>/<action>.schema.json`, with an `$id` following the existing
   convention.
2. `$ref` into `../../common/defs.schema.json` for shared types — do not redefine
   `assetPath`, `linearColor`, or anything else already there.
3. Add an example under `examples/` with `"x-schema": "domains/<domain>/<action>.schema.json"`.
4. `python tools/validate_schemas.py`.
5. Register the action in `docs/CAPABILITY_CATALOG.md` via proposal to WS-01.

## Cross-file references

Relative, resolved against each schema's absolute `$id`:

```json
{ "$ref": "../common/defs.schema.json#/$defs/assetPath" }
```

The validator builds its registry entirely from local files. Nothing is fetched over the
network, and a `$ref` pointing at an `$id` not present on disk is an error rather than a
silent skip. Every schema therefore needs an `$id`.

## Design notes worth knowing before you extend anything

Three decisions here are counterintuitive and were made deliberately:

1. **Graph `diagnostics` are mandatory on retrieval, not opt-in.** An agent asking for a
   graph is nearly always about to change it. Withholding "this subgraph is disconnected"
   until a second call reintroduces exactly the round-trip problem this project exists to
   solve.

2. **`metrics` are required on every response.** Round-trip reduction is the headline
   claim (`docs/WHY.md`). An optional metric does not get measured, and R-17 is
   discovering at the end that the gain never beat the ~5:1 baseline.

3. **Every `validation` boolean is nullable, and `checks_skipped` exists.** An
   unperformed check is `null`, never `true`. A response that stays silent about what it
   did not verify is a defect, not brevity — `AGENTS.md` rule 6.

## Versioning

`protocol_version` is `MAJOR.MINOR`. Additive, backward-compatible changes bump MINOR.
Anything that breaks an existing client bumps MAJOR — and a major mismatch is `rejected`
outright, never best-effort parsed.
