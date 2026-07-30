# UeremcpBlueprint

**Owner:** WS-06 (Blueprint Specialist). **Status:** P0 scaffolding — not POC A.

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

## Planned actions (P1+)

| Action | Schema | Notes |
|---|---|---|
| `read_graph` | `schemas/domains/blueprints/read_graph.schema.json` | One call → graph JSON + diagnostics |
| `submit_graph` | `schemas/domains/blueprints/submit_graph.schema.json` | `replace` / `patch` / create modes |

## Layering

```
UeremcpProtocol   (envelope, content hash — no editor)
       ↑
UeremcpBlueprint  (this module — ToolsetRegistry + Blueprint editor APIs)
```

Domain services must not include `ToolsetRegistry/` or `ModelContextProtocol/` outside
the thin toolset layer (ADR-0002 rule 4).

## Tests

Automation tests under `Private/Tests/`:

- `UeremcpBlueprint.Toolset.Ping`
- `UeremcpBlueprint.Toolset.Echo`
- `UeremcpBlueprint.Toolset.Register`

Run via editor automation or `tests/run_editor_tests.ps1` once the module is registered
in `UEREMCP.uplugin` (see `docs/proposals/ws-06-register-blueprint-module.md`).

## References

- `docs/research/RB-05-blueprint-graph-roundtrip.md`
- `docs/proposals/ws-06-patch-mode-and-impl-plan.md`
- `schemas/graph/graph.schema.json` (ADR-0004)
