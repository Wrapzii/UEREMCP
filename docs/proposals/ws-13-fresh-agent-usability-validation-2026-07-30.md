# WS-13 Fresh-agent usability validation — 2026-07-30

**Owner:** WS-13  
**Branch / worktree:** `ws-13-fresh-agent-usability-2026-07-30` @
`$UEREMCP_ROOT-ws13-fresh-agent-20260730`  
**Machine JSON:**
[`tests/integration/_logs/fresh_agent_usability_20260730.json`](../../tests/integration/_logs/fresh_agent_usability_20260730.json)

## Deploy SHA record

| When | SHA | Note |
|---|---|---|
| Session start | `8b67deb` | Junction → `UEREMCP-deploy-main`; merge of visual capture |
| Live editor session | `c24177c` | `[WS-01] Close cold visual capture and catalog it` — editor started here |
| Tip at report write | `28f0466` | Tip advanced while editor stayed loaded — **not** re-validated after restart |

Junction was **not** retargeted. Live claims below are against the editor process
loaded at `c24177c`.

## What a fresh agent would infer (docs only)

1. **Contradiction risk:** Some checkouts still present root README as Phase 0 /
   uncompiled scaffold while `docs/CAPABILITY_CATALOG.md` on deploy tip says POC
   A–E claimed. A naive agent may conclude “nothing to call.”
2. **Working path:** `docs/guide/agent-usage.md` + `capability-reference.md` +
   live `list_toolsets` / `describe_toolset` is enough to reach Ping without
   reading C++ — **if** the agent uses **full** toolset names from
   `list_toolsets` (e.g. `UeremcpCore.UeremcpReferenceToolset`) and camelCase
   `requestJson`.
3. **Opaque schemas:** Every UEREMCP AICallable exposes UHT
   `{ "requestJson": string }`. Envelope / specification fields are not in MCP
   schema; agents must load `schemas/` or guide examples.
4. **Time / calls to first success:** After editor was up,
   `list_toolsets` + describe pass + `Ping` → first success in **10** MCP calls
   (1 list + 8 UEREMCP describes + Ping). Earlier attempts failed with
   `WinError 10061` while the editor was down / restarting under concurrent
   consolidation.

## Success rate

**8 / 8 workflows passed** (100%) under the definition: achievable on safe
scratch / read-only observe paths with honest statuses, including expected
`rejected` / `partially_completed` when recovery worked.

| Workflow | MCP calls | Errors | Recovery | Status honesty | Evidence |
|---|---:|---:|---:|---|---|
| Discover + Ping/Echo | 12 | 0 | 0 | good | Ping/Echo envelopes |
| Blueprint read | 5 | 1 | 3 | good | `BP_CompleteRoundTripTransport` |
| Niagara create | 1 | 0 | 0 | excellent | `/Game/__UeremcpPoc/FreshAgent/NS_FreshAgent_Probe` |
| CaptureEffectFrames | 3 | 0 | 1 | excellent | `Saved/UEREMCP/VfxCapture/.../fresh-agent-capture-cold-1/` |
| Niagara inspect (no crash) | 1 | 0 | 0 | good | InspectSystem on FreshAgent probe |
| Idempotency retry | 1 | 0 | 0 | good | `no_change_required` + `idempotency.repeated_create_no_change` |
| Validation failure | 2 | 0 | 1 | excellent | `validate=false` → `rejected`; missing BP path → recover |
| cancel_job semantics | 1 | 0 | 0 | good | unknown job → `rejected` / `job not found` |

## Visual tool (`CaptureEffectFrames`)

| Question | Answer |
|---|---|
| Discoverable? | **Yes** — listed as `UeremcpValidation.UeremcpVisualCaptureToolset` with clear description |
| Schema explains render/tick/output? | **Partial** — description covers stage + `Saved/UEREMCP/VfxCapture` + pixel baseline; UHT input schema does not; cold path now returns ADR-0009 job + `capability_notes` to poll |
| Valid PNG + display? | **Yes** — PNG signature OK (157982 bytes); open files under `Saved/UEREMCP/VfxCapture/...` (response does not return image bytes) |
| Cold vs warm? | On `c24177c`, cold returns `partially_completed` + non-cancellable job; `GetJobResult` completed with `rendered_something=true`, `max_delta_lit_pixels=35` (IceWall). Recovery is obvious from notes. |

FreshAgent scratch capture also passed warm with `max_delta_lit_pixels=1179`.

## Adversarial documentation → live matrix (UEREMCP toolsets)

Safe probes only (list/describe + non-destructive). All eight UEREMCP toolsets
listed by `list_toolsets` were described live:

| Live toolset | Doc alignment | Probe |
|---|---|---|
| `UeremcpCore.UeremcpReferenceToolset` | aligned | Ping/Echo/CancelJob |
| `UeremcpBlueprint.UeremcpBlueprintToolset` | guide path drift (fixed) | ReadGraph |
| `UeremcpNiagara.UeremcpNiagaraToolset` | describe text vs Poc root soft mismatch | Create+Inspect |
| `UeremcpMaterial.UeremcpMaterialToolset` | aligned (describe) | describe only |
| `UeremcpGameplay.UeremcpGameplayToolset` | aligned (describe) | describe only |
| `UeremcpAnimation.UeremcpAnimationToolset` | aligned (describe) | describe only |
| `UeremcpTemplates.UeremcpTemplatesToolset` | aligned (describe) | describe only |
| `UeremcpValidation.UeremcpVisualCaptureToolset` | catalog available on tip; **schema file still missing** | CaptureEffectFrames |

Dangerous mutators (spell upsert, material create, template promote, graph
submit) were **not** executed.

## Usability blockers

1. **High — README drift across checkouts** (WS-01): Phase-0 wording on older
   tips vs live plugin.
2. **High — opaque `requestJson` schemas** (WS-01 / WS-05): load-bearing need
   for guides / `describe_action` (still planned).
3. **Medium — concurrent editor MCP refusals** during consolidation.
4. **Medium — Blueprint guide example path missing** — fixed in this branch.
5. **Low — capture image path formatting** (WS-11 handoff).
6. **Low — missing `capture-effect-frames.schema.json` on deploy tip** — added
   here for WS-01 adopt (unowned path warning under `check_ownership`).

## Fixes landed on this branch (WS-13)

- `docs/guide/agent-usage.md` — full MCP names, call shape, visual capture §9
- `docs/guide/capability-reference.md` — Blueprint discovery note, capture section
- `docs/guide/limitations.md` — visual capture ceiling
- `docs/guide/README.md` — coverage row
- `schemas/domains/validation/capture-effect-frames.schema.json` — **WS-01 please
  adopt / assign ownership**; catalog row already present on deploy tip
  `c24177c`+ (`available` with cold-job note)

### WS-01 handoff (catalog already done on tip)

Confirm catalog row stays in sync with schema file once merged. Suggested schema
path already matches WS-11 handoff:
`schemas/domains/validation/capture-effect-frames.schema.json`.

### WS-11 handoff

Prefer absolute or project-relative paths in capture responses instead of
`../../../../../../Users/...` forms.

## Ready for a new user agent?

**Conditionally yes**, if:

1. RE editor is running with the UEREMCP junction at current deploy tip  
2. The agent reads `docs/guide/` + catalog + live MCP discovery (not a stale
   Phase-0 README alone)  
3. Scratch stays under `/Game/__UeremcpPoc/FreshAgent/` or `/Game/__UeremcpTests/`  
4. Capture uses `options.validate=true` and polls `get_job_result` on cold
   `partially_completed`

Not production-ready; POC surfaces are usable for a careful fresh agent.
