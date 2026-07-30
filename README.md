# UEREMCP

A goal-level agent interface for Unreal Engine 5.8.

An AI describes a complete outcome — *"create a replicated fireball ability with VFX,
materials, gameplay tags, damage, and animation"* — submits **one** structured JSON
request, and receives **one** validated structured response with a complete change
manifest. Instead of several hundred primitive editor calls.

> **Status: POC A–E claimed on local tip (2026-07-30).** Goal-level create/inspect
> surfaces are implemented and live-verified against the RE project. This is
> **POC-complete, not production-ready** — see
> `docs/proposals/ws-01-hardening-consolidation-2026-07-30.md`. Closed live: D5
> multi-client, B10 rendered warm-pixel gate, cooperative `cancel_job`, durable
> idempotency Claim/Complete. Remaining: Epic MCP `notifications/cancelled`
> adapter limitation, durable-idempotency crash/migration caveats, metrics gaps.

## The problem

From the project owner, who has been living with the current tooling:

> "We were having a lot of problems doing harder, more intricate things — generating
> effects and templates from the beginning, as well as micromanaging nodes on
> characters. Even using my own toolset I was getting five-to-one efficiency, but tool
> calls would still fail because we didn't have the base data, or it would try to set
> things before getting information from it."

Two distinct problems live in that sentence, and the second is the important one:

- **Efficiency** — too many round trips. Slow and expensive.
- **Correctness** — the agent acts without adjacent context, so calls *fail*, assets
  break, and repair loops burn more calls than the original work.

Because every tool call re-sends the whole conversation, cost is **superlinear in call
count** and only linear in payload size. A 40 k payload sent once is cheap; a 1 k
payload re-sent ninety-nine times is not. That inverts the usual instinct: **return
more context, in fewer calls.**

Full reasoning and the cost model: [`docs/WHY.md`](docs/WHY.md).

## The approach

1. **Complete structured state.** Whole graphs — Blueprint, Niagara, Material, AnimBP,
   Control Rig — retrieved and submitted as validated JSON, with diagnostics included
   by default rather than on request.
2. **Verification before success.** Nothing reports `*_validated` unless it was
   re-read and checked. `"the tool returned OK"` is not success.
3. **Reusable patterns.** A template library that composes and varies known-good
   constructions, so *"a frost variant of the fireball"* is one operation.

## What already exists — and why it changes the design

UE 5.8 ships Epic's own agent stack, verified by source inspection
([`docs/GROUNDED_FACTS.md`](docs/GROUNDED_FACTS.md)):

| Component | What it gives us |
|---|---|
| `ModelContextProtocol` | A working in-editor MCP server (HTTP, `:8000/mcp`) |
| `ToolsetRegistry` | Tool declaration, JSON-Schema generation, async results, main-thread dispatch, file sandboxing, `AgentSkill` |
| `Toolsets` | 27 domain toolsets — Niagara, GAS, PCG, StateTree, UMG, physics, animation… |

So we do **not** build an MCP server, a transport, a schema generator, or a rollback
mechanism. We build the three things Epic did not: complete graph round-trip,
verification, and the pattern library. See
[`ADR-0002`](docs/adr/ADR-0002-host-model.md).

## Repository layout

```
AGENTS.md              Operating contract. Every agent reads this first.
docs/
  WHY.md               The problem and the cost model. Read before designing.
  GROUNDED_FACTS.md    Verified UE 5.8 API surface, with source citations.
  adr/                 Frozen architecture decisions.
  WORK_ALLOCATION.md   15 workstreams, disjoint file ownership.
  research/            One brief per workstream.
  audit/               What Epic and REAgentTools already do.
  POC_ACCEPTANCE.md    Binary acceptance criteria.
  ROADMAP.md           Phased plan.
  RISK_REGISTER.md     What could sink this.
schemas/               Frozen JSON contracts: envelope, graph, batch, templates.
Plugins/UEREMCP/       C++ editor plugin scaffold (not yet compiled).
tools/                 Schema validator, ownership guard.
```

## Getting started

**To launch the agent swarm:** copy-paste prompts are in
[`docs/SWARM_LAUNCH.md`](docs/SWARM_LAUNCH.md) — one orchestrator, a worker template,
and the four Wave 1 workstreams filled in.

**If you are an agent working this repo:** read [`AGENTS.md`](AGENTS.md), then find
your workstream in [`docs/WORK_ALLOCATION.md`](docs/WORK_ALLOCATION.md).

**If you are a human:** read [`docs/WHY.md`](docs/WHY.md), then
[`docs/ROADMAP.md`](docs/ROADMAP.md).

```bash
python tools/validate_schemas.py
```

## Where to start work

Four Wave 1 questions gate everything else. All four are answerable in days, by opening
a header or running code in the editor:

| Brief | Question | Risk |
|---|---|---|
| [RB-03](docs/research/RB-03-plugin-integration.md) | Can an out-of-tree plugin host `AICallable` envelope tools? | R-04 |
| [RB-05](docs/research/RB-05-blueprint-graph-roundtrip.md) | Can Blueprint graphs be reconstructed from JSON? | R-01 |
| [RB-06](docs/research/RB-06-sandbox-and-rollback.md) | Does `FileSandbox` cover package saves? | R-03 |
| [RB-02](docs/research/RB-02-epic-toolset-inventory.md) | What do Epic's 27 toolsets already do? | R-06 |

RB-05 is the one that decides whether the central thesis holds.

## Related

- `../REAgentTools` — prior art. 15 Python toolsets, ~5:1 measured efficiency. Its
  documented gaps are precisely this project's targets.
- `$UEREMCP_LEGACY_PROJECT` — the target project (UE 5.8).
