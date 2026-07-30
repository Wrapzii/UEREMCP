# UeremcpBlueprint

**Owner:** WS-06 (Blueprint Specialist). **Status:** P1 `read_graph` and bounded P2
scratch changed/unchanged replace implemented; A6 editor proof passed.

## Purpose

Blueprint-domain `AICallable` toolset for UEREMCP. One envelope in, one envelope out
(ADR-0003). P0 proves registration and the `FString` request-json parameter path; P1+
implements graph read/submit.

## Design constraints (do not violate)

1. **Keep primitives internal** — compose Epic `BlueprintTools` for optional reads when
   registered. Changed replace uses a bounded native C++ semantic writer because the
   MCP-enabled validation editor runs with `-DisablePython`; pin-level primitives remain
   hidden from the agent surface.
   [VERIFIED-RUNTIME: `tests/integration/_logs/poc_a_complete_round_trip_70cc348.json`]
   [VERIFIED: `EdGraphSchema.h:828`]
   [VERIFIED: `KismetEditorUtilities.h:169`]
2. **Hash structured JSON** — `content_hash` and `revision` use
   `FUeremcpContentHash` over ADR-0004 canonical graph JSON, not DSL text or display
   strings.
3. **Honest fidelity** — populate `fidelity.lossy_areas` from RB-05 when returning graphs.
   Known lossy areas include:
   - `multigate_no_dsl_roundtrip` — MultiGate on exec chains breaks DSL decompile
   - `timeline_special_spawn` — Timeline uses `|AddTimeline...` / `|Timeline` type_id
   - `math_expression_unproven`, `dsl_bind_elision`, `reroute_knots_elided`,
     `pin_type_as_display_string`, `node_guid_not_preserved`, `positions_not_semantic`
   - See `docs/research/RB-05-blueprint-graph-roundtrip.md` for the full list.

## P0 tools

| Tool | Purpose |
|---|---|
| `Ping` | Module liveness |
| `Echo` | Envelope parse/serialize round-trip without touching assets |

## P1 tools

| Tool / action | Purpose |
|---|---|
| `ReadGraph` / `read_graph` | One call → complete `graph.schema.json` + diagnostics + `content_hash`; empty variables/dependencies remain explicit arrays |
| `SubmitGraph` / `submit_graph` | Python-free scratch-only changed/unchanged `replace`; the native changed-write slice supports `EventBeginPlay → Branch → PrintString`, compiles/saves/re-reads, and returns complete evidence; optional `expected_after_write` selectors assert re-read nodes and links before `modified_and_validated`; dry_run-first; stale revisions rejected |

## Planned (P2+)

| Action | Schema | Notes |
|---|---|---|
| `submit_graph` | `schemas/domains/blueprints/submit_graph.schema.json` | Changed `replace`, `patch`, and create modes |

## Tests

Offline (no editor):

```bash
python Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/py/run_tests.py
```

Automation tests under `Private/Tests/` (editor; run via orch junction after merge):

- `UeremcpBlueprint.Toolset.Ping`
- `UeremcpBlueprint.Toolset.Register`
- `UeremcpBlueprint.Toolset.ReadGraphRoundTrip`
- `UeremcpBlueprint.Toolset.SubmitGraphValidation`
- `UeremcpBlueprint.Toolset.PocA6Reread`

Run via editor automation or `tests/run_editor_tests.ps1` once the module is registered
in `UEREMCP.uplugin` (see `docs/proposals/ws-06-register-blueprint-module.md`).

## References

- `docs/research/RB-05-blueprint-graph-roundtrip.md`
- `docs/proposals/ws-06-patch-mode-and-impl-plan.md`
- `schemas/graph/graph.schema.json` (ADR-0004)
