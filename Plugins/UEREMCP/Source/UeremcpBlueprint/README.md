# UeremcpBlueprint

**Owner:** WS-06 (Blueprint Specialist). **Status:** P1 `read_graph` implemented; P2 unchanged-replace validation implemented.

## Purpose

Blueprint-domain `AICallable` toolset for UEREMCP. One envelope in, one envelope out
(ADR-0003). P0 proves registration and the `FString` request-json parameter path; P1+
implements graph read/submit.

## Design constraints (do not violate)

1. **Compose Epic `BlueprintTools`** — preserve primitives (`create_node`, `connect_pins`,
   `read_graph_dsl`, `write_graph_dsl`, `compile_blueprint`, …). Do **not** rebuild
   pin-level primitives on the agent surface.
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
| `ReadGraph` / `read_graph` | One call → `graph.schema.json` + diagnostics + `content_hash` |
| `SubmitGraph` / `submit_graph` | Validates unchanged `replace` submissions and rejects stale revisions without mutation |

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

Run via editor automation or `tests/run_editor_tests.ps1` once the module is registered
in `UEREMCP.uplugin` (see `docs/proposals/ws-06-register-blueprint-module.md`).

## References

- `docs/research/RB-05-blueprint-graph-roundtrip.md`
- `docs/proposals/ws-06-patch-mode-and-impl-plan.md`
- `schemas/graph/graph.schema.json` (ADR-0004)
