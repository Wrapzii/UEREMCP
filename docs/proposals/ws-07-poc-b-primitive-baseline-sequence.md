# WS-07 → WS-11/WS-14: POC B fireball primitive baseline sequence

**Status:** Open — semantic equivalence defined; measured counts pending WS-14 trials  
**Owner:** WS-07 (sequence); WS-11 (harness execution); WS-14 (`docs/reviews/poc-metrics.md`)  
**Related:** `docs/proposals/ws-11-pocb-b1-b10-metrics-handoff.md`, `poc_b_mcp_fireball_request.json`

## Negative finding (no invented baseline)

No measured fireball-equivalent primitive baseline exists today. Do **not** substitute:

- UEREMCP `internal_operations` (46 on MCP B1) — domain-internal counter, not Epic MCP primitives
- REAgentTools ~5:1 headline — general benchmark, not this scenario
- One `execute_tool_script` MCP hop — batch container; inner primitive count must still be tallied
- Hand-count from six emitters/materials alone

[VERIFIED: `docs/proposals/ws-11-pocb-b1-b10-metrics-handoff.md` §Equivalent primitive baseline]

## Semantic target (must match UEREMCP run)

Same acceptance slice as `poc_b_mcp_fireball_request.json`:

| Slice | UEREMCP implementation | Baseline must reproduce |
|---|---|---|
| System | Duplicate `MinimalLightweight` → `/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline` | Same template + path root |
| Emitters | six roles: core, flame_shell, sparks, smoke, ribbon_trail, impact_burst | Same six emitter templates/names |
| User params | colour (primary/secondary), scale, intensity | Same exposed User.* variables |
| Materials | six inline `create_spec` roles (reuse_if_present) | Six MI assets with equivalent specs (WS-08 material goal ops or Epic MaterialTools/MaterialInstanceTools) |
| Renderer bind | mesh renderer material per role | Same bindings verified by re-read |
| Compile | RequestCompile + await UpToDate | One compile at end, not per edit |
| Save | package save | All created assets saved |
| Structural verify | post-create inspect (B7 slice) | Equivalent `GetSystemSummary` / topology re-read |

## Epic primitive sequence (inside one `execute_tool_script`)

Use `editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script`
[VERIFIED: `REAgentTools/Docs/NIAGARA_BATCHING.md:37-45`] with tool names from
`NiagaraToolsets.NiagaraToolset_System` [VERIFIED: `docs/audit/raw/plugins/NiagaraToolsets.json`].

Ordered minimum (count each `call_tool` inside the script as one primitive):

1. **Optional replace:** delete existing baseline asset if present (AssetTools or script helper)
2. `CreateNiagaraSystem` — from `/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight`
3. For each of six roles: resolve emitter template → `AddEmitter` (six calls)
4. User variables: `AddUserVariables` and/or `SetVariable` for colour, scale, intensity
5. For each role with mesh renderer: `SetRendererData` (material binding) — up to six calls
6. **Materials (outside or inside script):** six material-creation workflows equivalent to
   `specification.materials.*.create_spec` — typically WS-08 `create_vfx_material` goal ops
   or Epic `MaterialTools` + `MaterialInstanceTools` primitives per role. **Each inner
   material primitive counts.**
7. `GetSystemCompileState` poll loop until UpToDate (same observe-only pattern as UEREMCP MCP path)
8. Save Niagara system (+ material packages)
9. **Structural verify:** `GetSystemSummary`, `GetEmitterTopology` / inspect equivalent (B7)

### Anti-pattern trial (optional upper bound)

Separate MCP `call_tool` per step above (no batching) establishes a worst-case primitive
MCP round-trip count. Not required for POC acceptance but useful for reduction narrative.

## MCP round-trip accounting

| Workflow | MCP round trips | Primitive ops |
|---|---|---|
| UEREMCP B1 (measured) | **1** | **46** (`internal_operations`) |
| REAgentTools batched (to measure) | **1** script (+ material ops if separate) | **TBD** — tally script internals + material chain |
| REAgentTools anti-pattern (optional) | **TBD** | **TBD** |

## WS-11 trial checklist (from clean state)

1. Reset `/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline` and generated `/Game/__UeremcpPoc/Materials/*` deps.
2. Run baseline sequence with only pre-UEREMCP Epic/REAgentTools (+ WS-08 material tools if needed).
3. Record every MCP `call_tool`, every primitive inside `execute_tool_script`, retries/failures.
4. Capture client monotonic start/end; do not reuse editor log interval as `wall_clock_seconds`.
5. Repeat ≥3 trials; WS-14 records raw JSON in `docs/reviews/poc-metrics.md`.

## UEREMCP timing fields (after WS-07 land)

`CreateNiagaraEffect` emits optional `metrics.timing_ms`:

| Key | Meaning |
|---|---|
| `asset_creation` | Material resolve + system/emitter/param/bind work (ms, server monotonic) |
| `compilation` | Compile await observe loop |
| `save` | Package save |
| `validation` | Post-create inspect round-trip (when `options.validate`) |
| `server_total` | Full handler monotonic ms — comparable to WS-11 log lower bound, **not** client wall-clock |

Tokens and client wall-clock remain harness-owned (WS-11/WS-14).
