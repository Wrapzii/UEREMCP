# Why this project exists — the actual problem

**Owner:** WS-01. Read this before the master prompt. It is shorter and it is the
reason the master prompt was written.

## The problem as stated by the project owner

Direct from the person who has been living with the current tooling:

> "We were having a lot of problems doing harder, more intricate things, like
> generating effects and templates from the beginning, as well as micromanaging
> nodes on characters."
>
> "Even using my tool set that I developed, I was getting an efficiency of at least
> five to one, but we were still having issues where tool calls would fail because we
> didn't have the base data. We didn't have some kind of adjacent information, or it
> would try to go immediately and set things before getting information from it."

Two goals, in the owner's priority order:

- **(a)** speed up creation of both assets and logic
- **(b)** increase request efficiency — fewer requests, inputs, tool calls, outputs

## The distinction that should drive your design

There are two different problems here and they are easy to conflate:

| | Problem | Symptom | Fix |
|---|---|---|---|
| **Efficiency** | Too many round trips | Slow, expensive, hits context limits | Batching, goal-level operations |
| **Correctness** | Agent acts without adjacent context | Tool calls *fail*; broken assets; repair loops | Complete structured state |

The master prompt argues mostly from efficiency. **The owner's lived experience is
the correctness problem**, and it is the stronger justification.

An agent making 200 successful calls is slow. An agent that sets a property before
reading the surrounding struct produces a *broken asset*, then spends another 200
calls discovering and repairing it. The existing toolset already reached ~5:1 on
efficiency and still hit this wall — which tells you efficiency alone was never the
binding constraint.

**Design consequence:** when you trade payload size against completeness of context,
favour completeness. A response that costs 3k more tokens but prevents one blind
write has paid for itself many times over. This is why `understood`,
`reused_assets`, `unresolved_dependencies`, and mandatory graph `diagnostics` exist
in the schemas — they are all "adjacent information the agent would otherwise have to
ask for, or worse, guess at."

## Where the gain actually comes from

Be honest about the distribution. It is not uniform.

**Read side — large gain, low technical risk.** Returning complete structured state
in one response directly eliminates the "didn't have the base data" failure. Reading
graph structure through editor API is far more tractable than writing it. Most of the
reliability gain lives here.

**Write side — larger gain, high technical risk.** Whole-graph replace is what makes
node micromanagement disappear. Whether a Blueprint event graph can be faithfully
*reconstructed* from JSON via public UE 5.8 API — delegates, latent nodes, custom K2
nodes, macro instances — is unproven. `RB-05` gates this.

**Important hedge:** if whole-graph *write* proves partially blocked, the read side
still solves the owner's stated pain. The architecture degrades to "complete
inspection plus targeted patching" rather than collapsing. Do not let write-side
difficulty stall read-side delivery.

## Token economics — the cost model, and the objective function it implies

**Read this before optimising anything.** The naive model — "tokens ≈ sum of the
payloads" — is wrong, and designing against it produces the wrong system.

In an agentic loop, **every tool call re-sends the entire conversation context.** A
cached replay is cheaper per token, but the volume still moves. So:

```
total_tokens  ≈  N · C₀  +  r · N²/2

    N  = number of tool calls to complete the goal
    C₀ = base context size
    r  = average tokens each tool result adds to context
```

Cost is **superlinear in call count** and only **linear in payload size**. Worked
example, using the project owner's real figures:

| Approach | Calls | Payload each | ≈ tokens moved |
|---|---|---|---|
| Primitive loop: inspect/create/connect/compile | 100 | ~1 k | **~10 M** |
| Complete-state: retrieve graph, submit graph | 2 | one ~40 k | **~140 k** |

Roughly **70×**, not break-even. The reason is the part that is easy to get backwards:

> **A 40 k payload is cheap when it is re-sent once. A 1 k payload is expensive when
> it is re-sent 99 times.**

### Three design consequences

1. **Payload size is nearly free. Call count is the entire cost.**
   `response_detail` tiering exists to respect the **context window**, not to save
   tokens. The objective function is *minimise calls subject to fitting the window* —
   not *minimise bytes*. If you are ever choosing between one bigger response and two
   smaller ones, take the bigger one.

2. **Front-loading adjacent information is close to free.** Returning the surrounding
   struct proactively costs one payload, once. Making the agent ask costs a full
   context replay. This is why `understood`, `reused_assets`,
   `unresolved_dependencies`, and mandatory graph `diagnostics` are in the schemas.
   There is no tradeoff here — include the context.

3. **Failure rate is the dominant cost driver, and it compounds.** Weaker models do
   not reflect on a failed call; they immediately fire the next one. Every failure
   inflates `N`, and cost goes as `N²`. Verification-before-success and
   complete-state-up-front are therefore not quality niceties that happen to save
   money — they are the **primary cost lever**, attacking the quadratic term directly.

### What to promise

- **Tool calls: down by roughly an order of magnitude** on graph work.
- **Tokens moved per completed goal: down by considerably more**, because of the `N²`
  term. Do not quote a single multiplier; quote the measurement.
- **The uncounted win:** work that currently fails outright starts succeeding. No
  ratio captures this, and for "generating effects and templates from the beginning"
  it is probably the whole value.

### Scale context

The project owner moved roughly **3 billion tokens in one week**, at bursts of 5–10 M
tokens per minute. At that volume the `N²` term is the entire bill. A 10× reduction in
call count is worth far more than 10× in spend.

**Corollary for every workstream:** if you are weighing "return more information" against
"make the agent ask for it," return more information. The math is not close.

## Measure it

`metrics.mcp_round_trips` and `metrics.internal_operations` are mandatory on every
response (ADR-0003) precisely so this is measured rather than asserted.

Baseline to beat: **~5:1**, achieved by REAgentTools. Extend its existing harness
(`$RAT/Docs/BENCHMARK_REPORT.md`, `benchmark_ab_live.json`) rather than starting a new
one, so the numbers stay comparable. A new benchmark that cannot be compared to the
5:1 baseline tells the owner nothing.

Report three numbers per scenario, not one:

1. tool calls to complete the goal
2. total tokens to complete the goal, including failures and retries
3. **completion rate** — did the goal actually get achieved, verified

The third is the one that matters most and the one most likely to be omitted.

## What this means for scope

Epic already ships 27 domain toolsets, an MCP server, a file sandbox, and an
agent-skill primitive (`GROUNDED_FACTS.md`). Given that, the differentiator is
concentrated in three places:

1. **Complete graph round-trip** — read and write whole graphs as structured JSON
2. **Verification before success** — never report done without re-reading and checking
3. **The template/pattern library** — so "generate effects from the beginning" becomes
   "compose and vary known-good patterns"

Work outside those three is probably duplicating Epic. Justify it in `docs/audit/`
first.

## Sequencing recommendation

The master prompt names Niagara as one of the highest-priority domains. WS-01's
recommendation, recorded here so it is challengeable rather than silent:

**Do POC A (Blueprint round-trip) first.**

- It is the cheapest way to find out whether the central thesis holds.
- It addresses the owner's "micromanaging nodes on characters" pain directly.
- Niagara module stacks are the **least** node-graph-shaped domain in scope
  (`ADR-0004` open questions). Discovering that the shared graph representation does
  not fit is far cheaper in Blueprint than in Niagara.

Niagara follows immediately, informed by what POC A learns. This is not a
deprioritisation of Niagara — it is refusing to validate the architecture on its
hardest case first.
