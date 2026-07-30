# WS-01 full-use readiness (2026-07-30)

**Owner:** WS-01  
**Final tip:** `f7b64a1` (`[WS-01] Record full-use readiness verdict` on
`ws-01-full-use-integration`; this file’s containing commit).  
**Deploy model:** RE `Plugins/UEREMCP` junction →
`$UEREMCP_DEPLOY\Plugins\UEREMCP`  
**Do not push.**

## Verdict

**Ready for practical goal-level use within cataloged scopes — not
production-perfect.**

Agents can discover and call UEREMCP toolsets on RE, including Niagara
create/inspect (fail-soft), materials, Blueprint graph ops (scoped), templates,
jobs, and deterministic Niagara visual capture. Fresh agents have an explicit
routing contract (`docs/guide/tool-selection-policy.md` +
`tool-selection-contract.json`). Immutable Epic MCP cancel-adapter limits,
intentionally partial domain ceilings, and metrics gaps remain.

## Tip composition

| Source | SHA | What landed |
|---|---|---|
| Local main base | `a2bd21b` | Prior deploy alignment |
| WS-07 | `e751c80` | Niagara inspect fail-soft guards + tests |
| WS-11 | `5988a8d` | Visual capture toolset (warm path) |
| WS-01 follow-up | `c24177c` | Cold-capture ADR-0009 tick job + POC-B honesty assertion fix + schema/catalog |
| WS-13 | `7c96544` | Tool-selection policy, contract, examples, next-tool troubleshooting |
| This consolidation | tip containing this file | Catalog routing table, AGENTS/CLAUDE pointers, guide conflict resolution, readiness |

Uncommitted WS-11 visual work from `091d8bf` was superseded by `5988a8d` (hardened
MutatingDispatch / PNG reread path). Protocol docs from that earlier branch were
not required for the live tool.

## Tool readiness matrix

| Surface | Status | Evidence |
|---|---|---|
| `list_toolsets` / `describe_toolset` | ready | Live RE MCP; UEREMCP toolsets including VisualCapture present |
| Reference Ping / Echo / GetJobResult / CancelJob | ready | Describe live; cancel remains UEREMCP cooperative only |
| Blueprint ReadGraph / SubmitGraph | partial | Scoped CRT; not arbitrary complex graphs |
| Niagara InspectSystem | partial | Live inspect of probe succeeds; invalid-renderer fails soft (automation PASS) |
| Niagara CreateNiagaraEffect | partial | Structural + B10 warm-pixel PASS; not appearance perfection |
| Material CreateVfxMaterial / CreateProceduralTexture | partial | MutatingDispatch + Domain E3/E4 |
| Templates search / instantiate / promote | partial | Prefer instantiate over inventing ExecutePlan |
| ExecutePlan | partial | AICallable registered; durable idempotency caveats; not first-choice routing |
| `capture_effect_frames` | available | Warm: `no_change_required`, 44 changed lit px. Cold (fresh editor): `partially_completed` → poll → `no_change_required`, 44 changed lit px (`full-use-cold-delay-20260730`) |
| Gameplay CreateSpell | partial | POC D / D5 multi-client claimed earlier |
| Animation inspect / AnimBP read | partial | Read-only |
| Epic primitives | gap/discovery | Prefer for read-only discovery and catalog `planned`/`research` gaps only |

## Live evidence (RE, 2026-07-30)

1. **Build:** `REEditor Win64 Development` succeeded against the deploy junction.
2. **Discovery:** `list_toolsets` exposed
   `UeremcpValidation.UeremcpVisualCaptureToolset` and other UEREMCP toolsets;
   `describe_toolset` showed `CaptureEffectFrames(requestJson)`.
3. **Warm capture:** request `full-use-warm-20260730` → `no_change_required`,
   `rendered_something=true`, `max_delta_lit_pixels=44`, teardown complete.
4. **Cold capture (fresh editor):** request `full-use-cold-delay-20260730` →
   `partially_completed` + job → `GetJobResult` → `no_change_required`,
   `max_delta_lit_pixels=44`, `mcp_round_trips=2`. Zero-delay tick retry alone
   was insufficient; 0.25s ticker delay was required
   `[VERIFIED-RUNTIME: RE cold capture 2026-07-30]`.
5. **Inspect fail-soft:** automation
   `UEREMCP.Niagara.Inspect.InvalidRendererFailsSoft` PASS; live
   `InspectSystem` on `/Game/__UeremcpTests/NS_WS07_Probe` returned
   `partially_completed` without crash.
6. **POC-B honesty:** `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` with scaffold
   fixture → `UEREMCP_POC_B_GATE_OUTCOME=PASS` after aligning stale
   `never_claims` assertions with production diagnostics (status remains
   `partially_completed`; create honesty not overclaimed).

## Docs alignment

| Artifact | Result |
|---|---|
| `docs/CAPABILITY_CATALOG.md` | Routing table + `capture_effect_frames` available with cold-poll residual |
| `schemas/domains/validation/capture_effect_frames.schema.json` | Added; `validate_schemas.py` OK (26 schemas) |
| Guides + examples | Tool-selection policy/contract merged; visual capture section current |
| `AGENTS.md` / `CLAUDE.md` | Item 0b / 1b pointer to tool-selection policy |
| `check_tool_selection_contract.py` | 21 tools, 12/12 intents PASS |
| `check_guide_links.py` | Green after readiness file lands |

## Remaining limitations (explicit)

1. **Epic MCP `notifications/cancelled`** cannot reach ToolsetRegistry/AICallable
   work in UE 5.8 — immutable adapter limit. Use UEREMCP `cancel_job`.
2. **Durable idempotency** metadata/package non-atomicity; reclaimable in-progress
   claims; legacy Put/TryGetReplay sites.
3. **Graph / domain ceilings** remain `partial` (Blueprint complexity, Niagara
   topology lossiness, Animation authoring, metrics E7).
4. **Visual capture** proves pixel change vs empty stage only — not appearance,
   compile, or gameplay. Systems that do not render standalone still fail
   validation honestly.
5. **Tool-selection contract** improves fresh-agent odds; it cannot guarantee
   arbitrary LLM tool choice.
6. **Domain `describe_toolset` comment upgrades** proposed by WS-13 remain optional
   follow-ups for Templates/etc.

## Commands run

```text
python tools/validate_schemas.py                     → OK 26 schemas
python docs/guide/check_tool_selection_contract.py → 21 tools, 12/12 intents
python docs/guide/check_guide_links.py             → OK after this file
REEditor Win64 Development build                   → Succeeded
Focused automation (VisualCapture + Inspect + SixEmitter with scaffold) → PASS
```

## Fast-forward instruction

Local `main` should fast-forward to this tip. Junction remains on
`UEREMCP-deploy-main\Plugins\UEREMCP`. Do not push. Do not merge the 31 historical
superseded branches.
