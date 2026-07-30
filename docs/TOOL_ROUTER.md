# Tool Router — intent in, tools + schemas + order out

**Owner:** WS-11 (prototype). **Status:** offline prototype, measured against the
live 911-tool registry. **Not built into the plugin.**
Prototype: [`tools/route_prototype.py`](../../tools/route_prototype.py).

---

## 1. The problem, measured

An agent's first two calls accomplish nothing:

| Call | Cost | Outcome |
|---|---|---|
| `list_toolsets` | 17 KB / **~4,270 tokens** | 77 toolset names |
| `describe_toolset` | 0.6–29 KB | one toolset's tools |
| `call_tool` | — | usually rejected; envelope shape unknown |

Measured cost from cold start to **one** successful dry-run call: **5 MCP round
trips plus reading three schema files out of the repo**. An agent without repo
access cannot get there at all.

The surface is **911 tools across 77 toolsets**. Offering all of them on every
request is not viable, and lazy loading does not fix it — Epic's meta-tool
pattern (`list_toolsets`/`describe_toolset`/`call_tool`) reduces *payload*, not
*decisions*. The agent still reads 77 descriptions and guesses.

> Terminology correction: `GROUNDED_FACTS.md` §1.2 says "tool search already
> exists". That is **lazy loading**, not search or routing. Nothing in the
> registry searches tools — all 45 search-like tools search assets, CVars, tests,
> or plugins. Do not read §1.2 as "this is solved".

## 2. The shape

One call. Intent as plain text. Back comes an ordered plan with schemas:

```
route("make a spell effect with a helix, and show me what it looks like")
  -> { intent, confidence,
       envelope_contract,
       plan: [ {step, tool, purpose, why_here, input_schema, example}, … ] }
```

The agent then calls the listed tools directly. Discovery collapses from
2+ calls and ~25 KB to **one call and one payload**.

## 3. What makes it safe: the deterministic floor

Ranking is fuzzy. The *output* is not.

- **Candidates come only from `registry_snapshot.json`**, generated from the live
  registry by `tools/dump_tool_registry.py`. The router **structurally cannot
  invent a tool name** — the single most damaging thing a router could do, since
  a hallucinated name sends the agent into a guessing loop with no ground truth.
- **Lexical BM25, not embeddings.** Deliberate. A wrong ranking is visible and
  fixable ("matched: niagara, effect"); a wrong embedding is neither. The
  vocabulary here is small and technical, which is where lexical does well.
- **`confidence: none` is a valid answer.** A confidently wrong plan costs more
  than admitting uncertainty, because the agent stops questioning it and starts
  building on it. Below threshold it returns the `list_toolsets` fallback.
- **It never becomes the only path.** `list_toolsets` stays available.

## 4. Measured recall — and the real bottleneck

Against the live index, 7 intents, expectation declared before running:

| | result |
|---|---|
| top-1 | **2 / 7** |
| top-3 | **4 / 7** |

That is a poor score, and the cause is not the scorer. It is that **UEREMCP's
tool descriptions are written in architecture vocabulary, not task vocabulary**:

| Tool | Description |
|---|---|
| `ReadGraph` | *"action=read_graph — one MCP call returns graph JSON (ADR-0004) + diagnostics."* |
| `CreateNiagaraEffect` | *"Goal-level Niagara effect creation (POC B slice). Duplicate-and-modify via UNiagaraExternalEditUtilities…"* |

Mean description length: **UEREMCP 167 chars, everything else 228**. An agent
thinking *"add logic to a blueprint when the spell hits"* matches none of that,
while `ControlRigTools` talks about nodes and graphs at length and wins.

**So the router's recall is bounded by description quality.** That is the same
bottleneck already found three independent ways — it also blocks direct browsing
and first-call success. One fix, three payoffs, and it is not markdown: it is
`UFUNCTION` doc comments, which ship in the plugin and are exactly what
`describe_toolset` returns.

**Do not tune the scorer to raise this number.** Adding `logic→blueprint`,
`look→capture` aliases reaches 7/7 in five minutes and measures nothing except
the ability to write aliases against a known answer key. Aliases earn their place
only when a *measured* intent misses.

## 5. Known weaknesses, stated plainly

**Ordering is declared, not derived.** `DOMAIN_ORDER` asserts materials before
effects before logic before capture. The registry knows what tools exist, not
what depends on what. This is the only hand-maintained part and it can go stale.
It should eventually derive from `batch/plan.schema.json` dependencies and
template `construction_plan`s. A stale ordering is far less harmful than a wrong
tool name — the agent reorders and continues — but it is still the weak seam.

**The eval is 7 intents written by the author of the scorer.** It is a smoke
test, not a benchmark. A real one needs intents written by someone else, ideally
drawn from actual request history. At least one expectation is arguably wrong
already: "wire up a montage" expects `UeremcpAnimation`, but UEREMCP has no
montage-*authoring* tool — only `InspectMontage`. Routing to `SequencerTools`
may be correct, which means the eval is scoring the router down for being right.

**Stopwords mattered more than expected.** The first version ranked a material
tool top for a Niagara intent by matching `"a"`. IDF alone did not save it:
dividing by document length rewards short descriptions containing a stopword.
Anything replacing this scorer must handle that.

## 6. Relationship to focus mode

The router **subsumes** [focus mode](../tools/gen_focus_config.py). Both read the
same `SUPERSEDED` map, so they cannot disagree.

- Focus mode **hides** superseded primitives (140 tools). Risk: hiding a
  capability with no replacement.
- The router **demotes** them ×0.35, boosts goal-level ×1.6, and annotates the
  hit with `"superseded; prefer UeremcpNiagaraToolset"`.

Redirecting is strictly safer than hiding, because it cannot remove a capability.
**If the router ships, focus mode should become the fallback, not the primary
mechanism.**

Note also that focus mode hides 140 of 911 tools — 15%. It addresses the specific
fallback (Niagara/Material/Blueprint authoring primitives) but not scale.
`SequencerTools` alone is 140 tools. Only routing, or a positive `AllowedNames`
scope, addresses scale.

## 7. If this ships

1. Fix tool descriptions first. The router amplifies description quality; on
   today's text it would confidently misroute.
2. Expose as **one** AICallable, e.g. `UeremcpCore.UeremcpReferenceToolset.Route`,
   taking plain text.
3. Build the index at editor startup by walking
   `FToolsetRegistry::ForEachToolset` + `FToolset::ListToolNames`
   [VERIFIED: $TR/Public/ToolsetRegistry/ToolsetRegistry.h:88-91, Toolset.h:66-68],
   so it is generated in-process and can never drift from the live registry.
4. Keep `list_toolsets` reachable.
5. Log every routed intent with the chosen plan. That log is the only honest
   source of eval intents, and it is free.

## 8. Verdict

Worth building. It is the only intervention measured today that addresses the
911-tool scale problem rather than trimming its edges, and the deterministic-index
design removes the failure mode that would make a router dangerous.

But it is **second in line**. Descriptions first — they gate the router, direct
browsing, and first-call success simultaneously. Shipping the router onto today's
descriptions produces a confident 2/7, which is worse than no router, because the
agent trusts it.
