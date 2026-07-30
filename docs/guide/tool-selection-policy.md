# Tool selection policy (fresh agents)

**Owner:** WS-13. **Machine contract:**
[`tool-selection-contract.json`](tool-selection-contract.json).

This is the routing policy a **fresh** agent should use when choosing tools in the
RE editor MCP surface. It does **not** claim agents can be forced and cannot
guarantee arbitrary LLM tool choice. Prefer UEREMCP because the names,
descriptions, schemas, and examples below make the semantic path the accurate one —
not because of hidden prompts.

Grounded in [`docs/WHY.md`](../WHY.md) (cost model) and accepted ADRs 0002–0004,
0006, 0008–0009.

## 1. Prefer UEREMCP for create / modify / validate

| Intent class | Prefer | Avoid |
|---|---|---|
| Blueprint complete graph read | `UeremcpBlueprint…ReadGraph` (`action=read_graph`) | Epic `BlueprintTools` pin/node inspect loops |
| Blueprint graph replace | `…SubmitGraph` (`action=submit_graph`, `mode=replace`) | `create_node` / `connect_pins` / `write_graph_dsl` chains |
| Niagara create | `UeremcpNiagara…CreateNiagaraEffect` | NiagaraToolsets module primitives |
| Niagara inspect | `…InspectSystem` | Primitive topology scrapes alone when UEREMCP inspect exists |
| VFX material | `UeremcpMaterial…CreateVfxMaterial` | MaterialTools expression graphs |
| Known pattern / multi-asset from library | `UeremcpTemplates…InstantiateTemplate` | Inventing `ExecutePlan` MCP tool |
| Long job poll / cancel | `UeremcpReference…GetJobResult` / `CancelJob` | Assuming MCP `notifications/cancelled` alone |
| Niagara pixel evidence | `UeremcpValidation…CaptureEffectFrames` | Screenshot-driven authoring loops |

**Rule:** one semantic operation with a complete ADR-0003 envelope beats many
primitive calls (`AGENTS.md` rule 5; WHY cost model).

## 2. When Epic tools are appropriate

Use Epic (or REAgentTools) when:

- the need is **read-only discovery** and no UEREMCP action covers it
- [`docs/CAPABILITY_CATALOG.md`](../CAPABILITY_CATALOG.md) still lists the goal as
  `planned` / `research`
- the task is outside UEREMCP domains (world building utilities, etc.)

Do **not** hide useful Epic read-only tools globally. `FToolset::SetNameFilters`
exists `[VERIFIED: ToolsetRegistry/Toolset.h:59-60]` and is authorized by ADR-0002
to hide **internal mutation primitives** that semantic tools compose — not to blank
the editor. UEREMCP does **not** call `SetNameFilters` on this tip; selective
application is a WS-03 proposal, not a silent global filter.

## 3. How to call UEREMCP

```text
list_toolsets → describe_toolset(Ueremcp…) → call_tool(
  toolset_name = "Ueremcp<Domain>.Ueremcp<Domain>Toolset",
  tool_name    = "PascalCaseMethod",
  arguments    = { "requestJson": "<envelope JSON string>" }
)
```

`Ping` has no arguments. Semantic verb lives in envelope `action`, not only in the
MCP tool name. Worked envelopes:
[`capability-reference.md`](capability-reference.md),
[`examples/`](examples/).

## 4. Special selectors

| Selector | Choose |
|---|---|
| `execute_plan` | **Not** agent-facing AICallable. Use `InstantiateTemplate` or domain tools. |
| Templates vs direct domain | Template when a library match exists; else domain create/submit. |
| `read_graph` / `submit_graph` | Read complete → edit → submit with `expected_revision`. |
| `cancel_job` | Cooperative UEREMCP cancel via Reference toolset. |
| Visual capture | Existing Niagara system → `CaptureEffectFrames`; does not author assets. |

## 5. Envelope fields that affect routing

| Field | When it matters |
|---|---|
| `options.dry_run` | Destructive / replace — dry-run first unless explicitly opting out |
| `options.validate` / `compile` / `save` | Required for `*_validated` claims |
| `expected_revision` | Any modify after a read |
| `idempotency_key` | Retries of the same logical attempt |
| `options.timeout_ms` | `>0` may yield `partially_completed` + job → poll |

## 6. Ambiguity notes (honest)

- Epic and UEREMCP both appear in `list_toolsets`. Prefer names starting with
  `Ueremcp` for the intents in §1.
- Catalog `partial` means usable with ceilings — read
  [`limitations.md`](limitations.md).
- `capture_effect_frames` is catalog-registered and live-verified. A cold
  renderer may return `partially_completed` and need one `get_job_result` poll;
  a second zero-pixel result is honest `failed_validation`.
- `ExecutePlan` is registered AICallable but not the preferred first-choice
  surface — prefer `InstantiateTemplate` or domain tools.
- Live `describe_toolset` comments are owned by domain WSs; this policy is the
  static source of truth until those descriptions are updated per the WS-13
  proposal.
- This policy cannot guarantee arbitrary agent/LLM tool choice.
