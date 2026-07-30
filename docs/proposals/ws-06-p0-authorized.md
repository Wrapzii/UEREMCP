# WS-06 P0 authorized — Phase 1 exit kickoff

- **From:** WS-01
- **To:** WS-06
- **Date:** 2026-07-30
- **Status:** **Authorized** — begin implementation on `ws-06-blueprint`

## Context

Phase 1 exited 2026-07-30 with mitigated gates (`docs/ROADMAP.md`):

| Gate | Verdict |
|---|---|
| R-04 | Closed — MCP Ping/Echo on RE |
| R-03 | Mitigated — Content/ full-Discard rollback |
| R-01 | Mitigated — RB-05 thesis answered; bridge = POC A |
| R-06 | Mitigated — priority audit + 12 runtime schemas |

R-04 blocker in `ws-06-patch-mode-and-impl-plan.md` is **removed**. Implementation plan
there remains the accepted architecture.

## Authorized scope (P0 only)

Implement **Phase P0** from `docs/proposals/ws-06-patch-mode-and-impl-plan.md`:

1. `UeremcpBlueprint` module + `UUeremcpBlueprintToolset` with one envelope echo tool
   proving R-04 `AICallable` JSON-string shape on RE.
2. Domain schemas: `read_graph` / `submit_graph` **specification stubs** under
   `schemas/domains/blueprints/` (extend `specification` only).
3. `python tools/check_ownership.py --ws WS-06` and `python tools/validate_schemas.py`
   green.
4. Tests: module loads; echo tool round-trips envelope parse (no graph walk yet).

## Do not start yet (P1+)

- C++ graph walk / `graph.schema.json` read path (POC A1–A3)
- DSL translator / replace path (POC A4–A11)
- Patch mode execution

Those follow P0 landing and remain POC A deliverables.

## Owned paths

WS-06 owns `Plugins/UEREMCP/Source/UeremcpBlueprint/**` and
`schemas/domains/blueprints/**`. Register module in uplugin via proposal to WS-03 if
needed (`docs/proposals/ws-03-blueprint-module.md` — create if absent).

## Success criteria for P0 handoff

- [ ] Plugin compiles on RE with `UeremcpBlueprint` loaded
- [ ] One `AICallable` envelope echo tool discoverable via MCP `describe_toolset`
- [ ] Schema stubs validate
- [ ] Ownership + schema CI green
- [ ] Honest status: scaffolding only — not POC A

## References

- `docs/research/RB-05-blueprint-graph-roundtrip.md`
- `docs/POC_ACCEPTANCE.md` POC A criteria
- `docs/proposals/ws-06-patch-mode-and-impl-plan.md`
