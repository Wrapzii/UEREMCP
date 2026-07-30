# Benchmark Protocol — Baseline vs UEREMCP

**Owner:** WS-01 (protocol) / WS-11 (execution). **Status:** ready to run on whatever
is currently `available`/`partial` in `docs/CAPABILITY_CATALOG.md`; expand as more
domains close.

## What this is, and what it is not

`docs/POC_ACCEPTANCE.md` answers *"does it work?"* — binary, verified, gated. This
document answers a different question: *"how much better is it than what we had?"*
It measures the efficiency claim the whole project is justified by
(`docs/WHY.md`'s cost model, the ~5:1 REAgentTools baseline it commits to beating).

This is not a replacement for POC acceptance. A scenario that "wins" the benchmark but
was never verified correct is not a result — it is noise. Every trial in both arms
gets the same correctness check before its numbers count.

## The comparison

**Arm B (Baseline):** the agent has access to everything that existed *before*
UEREMCP — raw Epic toolset primitives (`docs/audit/epic-toolsets.md`) and REAgentTools'
composite tools. **No UEREMCP actions.** This is deliberately "whatever you were
actually using" — the exact setup that produced the ~5:1 result with the failure mode
described in `docs/WHY.md` ("tool calls would fail because we didn't have the base
data").

**Arm U (UEREMCP):** the agent has UEREMCP's goal-level actions available (and may
still fall back to primitives if it chooses — don't forbid it, just record which it
used).

Same goal, same starting project state, same model where possible, two different tool
surfaces. The gap between them is the number that matters.

## Ground rules — a benchmark you can't trust is worse than no benchmark

1. **Reset scratch state before every trial.** Delete everything under
   `/Game/__UeremcpTests/Benchmark/` before each run. A run that benefits from a
   previous run's leftover assets is not measuring what you think it's measuring.
   **Use `/Game/__UeremcpTests/`, not `/Game/__UeremcpBenchmark/`** — confirmed live
   2026-07-30: `create_vfx_material` rejects any target outside `/Game/__UeremcpTests/`
   with `"create_vfx_material only writes under /Game/__UeremcpTests/ until WS-12 tier
   policy extends allowed roots"`, `metrics.internal_operations: 0`. That is WS-12's
   security allowlist (`docs/research/RB-13-security.md`), and it is real and
   enforced — a rejected call here is the gate working, not a bug. Use a `Benchmark/`
   subfolder under the allowed root to keep results separate from WS-11's own
   fixtures.
2. **Fresh agent session per trial, no cross-contamination.** The agent must not see
   the other arm's transcript, tool list, or approach. Give both arms the identical
   natural-language goal text below — verbatim, not paraphrased per-run.
3. **N ≥ 3 trials per scenario per arm.** Agent behavior is stochastic, especially in
   the baseline arm where completion itself is uncertain. One run is an anecdote.
4. **Log every call, including failures and retries.** The retries are most of the
   cost (`docs/WHY.md`'s `N²` term). A report that only shows the successful final
   path understates the baseline arm's true cost, sometimes by a lot.
5. **A run that fails to complete is still a data point.** Record it as
   `completed: false` with the failure mode. Do not discard failed baseline runs —
   they are the whole reason this project exists.
6. **Verify correctness the same way for both arms.** Re-read the asset, confirm
   compilation, confirm the specific properties requested. "The agent said it was
   done" is not verification, in either arm.

## Data schema

One row per trial. CSV template at
[`tests/benchmark/results_template.csv`](../tests/benchmark/results_template.csv).

| Field | Meaning |
|---|---|
| `run_id` | Unique per trial |
| `date` | |
| `scenario_id` | `T0-actor-spawn`, `T1a-material`, etc. (below) |
| `arm` | `baseline` \| `ueremcp` |
| `agent` | e.g. `grok-4.5`, `composer-2.5`, `claude-sonnet-5` — record the actual model |
| `tool_calls_total` | Every tool invocation, success or fail |
| `tool_calls_failed` | Subset that errored or rejected |
| `retries` | Calls that re-attempted the same sub-goal after a failure |
| `mcp_round_trips` | UEREMCP arm: read directly from `response.metrics.mcp_round_trips` (ADR-0003) — self-reported, no counting needed. Baseline arm: count manually, since primitive tools carry no such field. |
| `internal_operations` | UEREMCP arm: from `response.metrics.internal_operations`. Baseline arm: `n/a`. |
| `tokens_input` / `tokens_output` / `tokens_total` | From the agent harness's own usage reporting |
| `wall_clock_seconds` | Start to completion or to give-up |
| `completed` | Boolean — did it pass the scenario's acceptance criteria, verified |
| `verification_method` | How you checked (re-read asset, compiled, screenshot, etc.) |
| `failure_mode` | Free text if `completed: false`, or if it succeeded only after retries — this field is where the interesting story usually is |

## Scenarios

Each has: the exact prompt to give the agent (same text both arms), acceptance
criteria, and current gate status. Status reflects the catalog as of this writing —
re-check `docs/CAPABILITY_CATALOG.md` before running; a `research`/`planned` action has
no UEREMCP arm yet and that scenario should be skipped or run baseline-only.

---

### T0 — Harness sanity (control scenario)

Deliberately trivial. Primitive tools are already reasonably efficient at single-actor
spawns — this scenario exists to keep the benchmark honest by showing where UEREMCP
does *not* win much, not to inflate the headline number.

**T0a — single actor.** *"Spawn a StaticMeshActor using the default cube mesh at
location (0, 0, 100), rotation (0,0,0), scale 1, and label it `Benchmark_Cube_01`."*

**T0b — batch of two.** *"Spawn two StaticMeshActors 200 units apart along X, both
using the default cube mesh, labeled `Benchmark_Cube_A` and `Benchmark_Cube_B`."*

Acceptance: actor(s) exist at the specified transform with the specified label,
verified by re-querying the level.

Gate: available both arms today.

---

### T1a — Material: elemental projectile core ("Ice" variant)

Not the Fireball material already used throughout the repo's examples — pick something
distinct to avoid measuring a case the system was tuned against.

**`purpose` must be one of the currently wired values.** Confirmed live 2026-07-30:
only `elemental_projectile_core`, `elemental_projectile_trail`, `fireball_core`, and
`fireball_ribbon_trail` are implemented. An earlier draft of this scenario used
`force_shield_core`, which is unimplemented — the service correctly returned
`partially_completed` with `internal_operations: 0` and did no work rather than
guessing. That is honest behavior, not a bug, but it means "force shield" is not
runnable yet. **Use `elemental_projectile_core` with `element: "ice"`** for a
Fireball-distinct trial that is actually implemented; re-check
`schemas/domains/materials/create_vfx_material.schema.json` and
`templates/elements/` before each run, since wired purposes/elements expand over time.

*"Create a VFX material for an ice-elemental projectile core: fresnel rim, dynamic
color, dynamic intensity, using the ice elemental preset. Save it as a reusable master
material plus one instance."*

```json
{
  "action": "create_vfx_material",
  "target": { "asset_path": "/Game/__UeremcpTests/Benchmark/Materials/MI_Ice_Core" },
  "mode": "create_or_update",
  "specification": {
    "purpose": "elemental_projectile_core",
    "element": "ice",
    "features": ["fresnel", "dynamic_color", "dynamic_intensity"]
  }
}
```

Acceptance: master material + instance exist; fresnel and the two dynamic parameters
(`intensity`, `color`) are present and exposed; master **saved to disk** (see live
finding below — do not assume save succeeded just because the graph was built);
material compiles.

Gate: `create_vfx_material` is `partial`. **Live run 2026-07-30 against a real editor
session** (not `RE` — the `visualtest` project, per the returned asset paths) returned
`status: partially_completed`, `metrics.internal_operations: 42`,
`metrics.mcp_round_trips: 1`, with the specific failure
`"MI parameters applied and verified but save failed for
'/Game/__UeremcpTests/Materials/MI_Ice_Core_Benchmark'"` and
`interpretation_notes` confirming: `"Failed to save master
'/Game/__UeremcpTests/Materials/Masters/M_Ueremcp_ProjCore_325B49C6' to disk."` The
in-memory graph was built correctly (features wired, element preset applied) but disk
persistence did not complete. **This looks like a real, reproducible bug, not a
one-off** — flag it to WS-08/WS-11 before running this scenario at volume, or every
UEREMCP-arm trial on this purpose will log as `partially_completed` for the same
underlying reason rather than N independent data points.

---

### T1b — Niagara: "Arcane Portal" (multi-component, not a single particle spawner)

Explicitly not a simple particle sprinkle (e.g. falling snow) — a portal needs a ring
silhouette, swirling distortion, particles, and a pulsing light, i.e. multiple
coordinated components, closer to the complexity this project is meant to collapse into
one call.

*"Create a new reusable Niagara effect: a circular magical portal. It should have a
ring or disc silhouette with swirling energy distortion, an inward-drifting particle
stream suggesting a vortex, and a soft pulsing light. Primary color deep violet,
secondary color pale cyan. Save it as a new template, not tied to any specific spell."*

Acceptance: system exists with the requested components as separate emitters,
renderers bound to valid materials, compiles, saved. If claiming template promotion,
verify a second instantiation actually varies (not a hand-edited copy).

Gate: `create_niagara_effect` / `inspect_system` are `partial` — Niagara Create/Inspect
have PASS editor evidence. The B7 *scaffold test* is still failing, but that is a
narrower internal regression test, not this scenario — run it, and record whatever
actually happens, including if it fails in a new way B7 hasn't already caught.

---

### T1c — Blueprint: small logic change (the actual stated pain point)

Two variants. Run the safe one first; only run the stretch variant once you're
comfortable pointing agents at real project content (or clone it first).

**T1c-safe** (scratch Blueprint): *"In a new Actor Blueprint `BP_Benchmark_Logic`, add
a BeginPlay event that branches on a new public boolean variable `bBenchmarkFlag`
(default true): if true, call Print String with 'Flag On'; if false, call Print String
with 'Flag Off'. Compile and save."*

**T1c-stretch** (real content): the same kind of small, well-specified logic addition,
but against `BP_RECharacter` or another real project Blueprint — this is the actual
"micromanaging nodes on characters" pain from `docs/WHY.md`, not a proxy for it. Define
the exact change before running, so both arms are held to the same target and you can
verify it precisely.

Acceptance: the graph contains the requested nodes correctly wired, compiles, and a
re-read of the graph confirms it (this is `ReadGraphRoundTrip`-shaped verification).

Gate: `UeremcpBlueprint.Toolset` (`SubmitGraphValidation`, `ReadGraphRoundTrip`) has
PASS editor evidence. Full POC A (whole-graph replace with stable `content_hash`
across an unrelated retrieve) is not yet closed — this scenario tests small patches,
which is a fair and currently-supported comparison, not a claim of full POC A.

---

### T2 — Cross-domain batch: material + effect + ability, one request

The flagship scenario. Reuses the shape already specified in
[`schemas/examples/batch-fireball-ability.json`](../schemas/examples/batch-fireball-ability.json)
so it doesn't need to be redesigned — but for the actual benchmark run, use a
**different** spell so this isn't the same example the schema authors already hand-tuned.
Suggestion: a frost/ice variant with different delivery (e.g. a cone AoE instead of a
projectile) to also exercise a different `delivery.type`.

*"Create a new frost nova ability: an instant area-of-effect spell centered on the
caster, radius 400, that visually shows an expanding ring of ice crystals with a frost
mist, deals 30 damage, and applies a 'Chilled' status. Server-authoritative, replicated.
Wire it into the project's ability/spell system."*

Acceptance: material(s), Niagara effect, and the spell/ability row all exist, correctly
cross-referenced (not just present but pointing at each other), compiles, and
`validate_system` (or equivalent manual check) confirms dependencies resolve.

Gate: `create_spell` is `partial` — **preflight-only**, module registration and the
WS-12 mutator queue are pending per the catalog. Run **baseline arm only** for now and
record it; add the UEREMCP arm once `create_spell` moves past preflight. Don't skip
collecting the baseline number just because the comparison arm isn't ready — baseline
data doesn't get easier to collect later.

---

### T3 — The headline scenario (aspirational, do not run yet)

From `docs/POC_ACCEPTANCE.md`: inspect a whole player spell system in one call,
identify what's broken, add a new spell based on an existing one, create missing
Niagara/material patterns, wire into the ability system, configure replication, compile,
validate, return one change manifest. One logical job.

This is the top of the scale, not a near-term benchmark target. Gate: run only once
POC A, B, and D are closed. Listed here so it's on the roadmap for the *final* "everything's
done" run the way you originally asked for — this is that run.

---

## Execution protocol

1. Confirm scenario gate status against `docs/CAPABILITY_CATALOG.md` immediately
   before running — statuses change as the swarm lands fixes.
2. Reset `/Game/__UeremcpTests/Benchmark/` to empty.
3. Run baseline arm, N ≥ 3 trials, fresh session each time.
4. Reset scratch state again.
5. Run UEREMCP arm, N ≥ 3 trials, fresh session each time.
6. Verify every trial's output against the scenario's acceptance criteria — not just
   "did the agent say it finished."
7. Log every field in the schema above, per trial, to the CSV.
8. Roll up per scenario/arm: mean tool calls, mean tokens, completion rate, and the
   ratio between arms.

## Reporting

Per scenario, report at minimum:

| Scenario | Arm | Trials | Mean calls | Mean tokens | Completion rate | Speedup (calls) |
|---|---|---|---|---|---|---|
| T1a-material | baseline | 3 | | | | — |
| T1a-material | ueremcp | 3 | | | | ×N |

**Completion rate is the number to lead with, not the speedup ratio.** A 10× call
reduction on a scenario the baseline arm only completes 40% of the time is a different
(better) story than the ratio alone tells — say both.

## Comparability to the existing baseline

This extends, rather than replaces, the ~5:1 figure already established by
REAgentTools (`docs/WHY.md`, `$RAT/Docs/BENCHMARK_REPORT.md`,
`benchmark_ab_live.json`). Where a scenario here has a REAgentTools-era equivalent
result, cite it alongside the new baseline-arm number so the two are comparable rather
than two different baselines.


---

# Part II — Agent Usability

**Owner:** WS-11. **Status:** design + one pilot data point (§8).
**Part I is the efficiency benchmark above.**

---

## 1. What this measures, and why it is not the benchmark

`BENCHMARK_PROTOCOL.md` answers *"given a task, how much does doing it cost?"*
It hands the agent the task and counts round trips.

This protocol answers a different question: **"given a goal in the user's own
words, does the agent work out which tools to call?"** Nobody tells it. The tool
surface has to teach it.

These fail identically from the outside — the agent flails and the task doesn't
get done — but the fixes are opposite:

| Symptom | If it's a *cost* problem | If it's an *affordance* problem |
|---|---|---|
| 40 tool calls | batch them, add a goal-level composite | rename/redescribe so the right one is obvious |
| Task not completed | add missing capability | the capability exists and was not found |

Building the wrong fix is expensive, so the eval has to separate them. The
discriminator is in §6.

**Second discriminator, equally important:** run every scenario across **at least
two models** (Grok 4.5, Composer 2.5, and one Claude). If every model fails the
same way, the tool surface is at fault and it is our bug. If one model fails
where others succeed, it is a model-capability difference and redesigning the
tools will not fix it. Single-model results cannot tell those apart and should
not be acted on.

## 2. The one inviolable rule: blind

**The agent is never told which tools exist, which toolset to use, or that a
relevant tool exists at all.** It gets the user's goal and the standard system
prompt, nothing more.

Any hint contaminates the run permanently. "Use the Niagara toolset" converts an
affordance test into a cost test. If a run is contaminated, discard it — do not
"adjust for" the hint.

Corollary: the operator running the trial must not answer questions mid-run
except as the *user* would (see clarification handling in §4).

## 3. What gets recorded

Per run, alongside the `BENCHMARK_PROTOCOL.md` fields:

| Field | Why it matters |
|---|---|
| `first_tool_called` | The single most actionable datum. What does the surface *suggest*? |
| `first_call_correct` | bool — was the first substantive call on the right tool? |
| `calls_to_first_success` | Flailing length before traction. |
| `discovery_calls` | `list_toolsets` / `describe_toolset` count, and their token cost. Pure overhead paid before work. |
| `discovery_tokens` | `describe_toolset` measured at ~29 KB for one Niagara toolset. Counted separately or it hides inside "task cost". |
| `wrong_tool_events` | Each with the taxonomy code from §6. |
| `hallucinated_tools` | Tools the agent invented. Direct evidence of a naming/doc mismatch. |
| `clarifying_questions` | Asked before acting, on a genuinely ambiguous brief. |
| `unprompted_verification` | Did it verify without being told? `AGENTS.md` rule 6 is a *behaviour* we should measure, not just assert. |
| `false_success_claim` | Claimed done when it wasn't. **The most dangerous outcome and the one to weight heaviest.** |
| `artifact_delivered` | Did the user actually receive something to look at? |

`false_success_claim` deserves special handling: a run that completes fast with a
confident wrong answer is **worse than a run that fails honestly**, and any
scoring that averages them is broken. Report it as its own count, never folded
into a success rate.

## 4. Handling ambiguity honestly

The headline scenario (§5) is deliberately underspecified, because real requests
are. That creates three distinct outcomes that must not be scored the same:

1. **Asked a clarifying question, then built it** — *best*. Correct behaviour on
   an ambiguous brief.
2. **Assumed, stated the assumption, built it** — *acceptable*. Matches the
   project's own delivery norm.
3. **Assumed silently and built the wrong thing** — *failure*, even if the
   output is pretty.

Score these separately. An eval that rewards only "produced an asset" trains the
swarm toward outcome 3.

When an agent asks a clarifying question, the operator answers **as the user
would** — briefly, in domain language, without naming tools.

## 5. Scenario ladder

Run in order. Stop escalating when a rung fails; the failure rung is the finding.

**U0 — Unambiguous, single domain.** *"Make me a material for an ice shard —
translucent, blue, with fresnel edges."* One domain, standard vocabulary, exists
today. If U0 fails, nothing above it means anything.

**U1 — Unambiguous, requires inference about verification.** *"Change the fireball
to be more orange, and show me it worked."* Tests whether "show me" routes to
the capture tool without being named.

**U2 — Cross-domain, moderate ambiguity.** *"Give the ice wall spell an impact
effect when it shatters."* Requires the agent to notice this spans Niagara +
Blueprint and to find the existing `NS_Spell_IceWall_Shatter` rather than build a
duplicate. **Discovery-of-existing-assets is a first-class result here** — see §8.

**U3 — The headline.** Verbatim, §5.1.

### 5.1 U3 — the headline brief

Given to the agent exactly as written, with no glossary:

> Make a new effect. It needs to look like a spell being cast with a helix
> windscreen around a circle that's about fifty meters tall, with shadows around
> it, using real particles, spawning in from a beam of light. Then show me what
> it looks like.

This is a good test precisely because it is *not* clean. Decomposition an ideal
agent would reach:

| Phrase | What it likely means | Trap |
|---|---|---|
| "helix windscreen" | a helical ribbon/spiral column | non-standard term; `NS_Spell_IceWall_CastIso_Helix` already exists and should be found, not reinvented |
| "around a circle" | ground sigil at the base | `M_Magecraft_Ice_Circle` exists |
| "fifty meters tall" | **5000 Unreal units** | unit conversion; getting this wrong is silently plausible |
| "shadows around it" | shadow-casting light, or dark contact/occlusion | genuinely ambiguous — should trigger a clarifying question |
| "real particles" | GPU/CPU particle sim, not a flipbook or mesh fake | tests whether the agent understands the distinction |
| "spawning in from a beam of light" | a beam emitter that precedes and seeds the formation | ordering/timing, not just presence |
| "show me what it looks like" | `capture_effect_frames` + contact sheet | tests §5's U1 inference at full complexity |

**Do not** hand the agent this table. It is the operator's scoring key.

Scoring U3 is per-clause, not pass/fail: seven clauses, each `met` / `partial` /
`missed` / `asked`. A run that meets four and asks about "shadows" is a better
result than one that silently meets five and inverts the scale.

## 6. Wrong-tool taxonomy — the actionable output

Every wrong call gets a code. The distribution tells you what to fix, and this is
the part that turns the eval into a work queue.

| Code | Meaning | The fix |
|---|---|---|
| `W-NAME` | Called a plausible name that doesn't exist | Rename the real tool, or add an alias |
| `W-DOC` | Tool exists and does the job; description didn't say so | Rewrite the description |
| `W-SHAPE` | Right tool, malformed arguments | Fix schema/examples in the error message |
| `W-GRAIN` | Reached for a primitive when a composite existed | The composite is under-advertised |
| `W-DUP` | Rebuilt something that already existed | Discovery gap — see `visual_loop_tool_notes` |
| `W-ORDER` | Right tools, wrong sequence | Encode the ordering in a plan/composite |
| `W-NONE` | No tool exists for what it needed | Genuine capability gap — the only code that means "build something" |

**Only `W-NONE` justifies new capability.** Everything else is a documentation,
naming, or packaging fix — much cheaper, and the ones we have evidence for are
mostly not `W-NONE`.

## 7. Running it

1. Fresh agent session per scenario. No carry-over context — a second run is not
   blind.
2. Editor at a known-good state; record the plugin branch and commit.
3. Record the full transcript. `first_tool_called` is not reconstructable later.
4. Three runs per (scenario × model). Tool inference is stochastic; a single run
   measures luck.
5. The operator does not intervene except to answer clarifying questions in the
   user's voice.

Log into `tests/benchmark/results_template.csv` extended with the §3 fields.

## 8. Pilot data — subject 0

This session was an accidental U2-class run: an agent (me) with the live tool
surface, no guidance about which tools existed, tasked with capturing a VFX. The
results are unflattering and therefore useful.

| Event | Code | Detail |
|---|---|---|
| Called `inspect_system` on `UeremcpNiagaraToolset` | `W-NAME` | Does not exist. Inferred from prose in our own docs. |
| `GetSystemSummary` with bare asset path, twice | `W-SHAPE` | Needs nested `{"system":{"refPath":"/Game/X.X"}}` **and** the full `Package.Asset` form |
| Built a capture stage from scratch | `W-DUP` | `Scripts/capture_ice_wall_baseline0.py` already did the whole job |
| Never found `Scripts/` | `W-DUP` | Searched `Content/Python/` only |
| Reached for `seek_to_desired_age` | `W-DOC` | `advance_simulation` + `SetForceSolo` was correct; nothing said so |
| Missed `SetForceSolo` entirely | `W-DOC` | Non-discoverable precondition; **cost most of the session** |
| Concluded "PIE may be required" | *false inference* | Would have been recorded as a finding and propagated |

Discovery cost: `describe_toolset` ~29 KB per toolset, several called.
`unprompted_verification`: yes — pixel stats and a control sphere, unprompted.
`false_success_claim`: **no**, but near-miss — the "PIE required" conclusion and the
retracted "tick-boundary rule" would both have entered the docs as facts.

**Distribution: 3× `W-DUP`/`W-DOC` on things that existed, 0× `W-NONE`.** Not one
failure was a missing capability. That is the headline: on this evidence the tool
surface's problem is **discoverability, not coverage** — which is the cheap
problem to have, and it means the swarm's instinct to build more tools is
probably wrong.

Caveat: n=1, one model, not blind (I chose my own task), and I am not one of the
models that will run the swarm. Treat as calibration for the protocol, not as a
result.

## 9. What §8 already suggests fixing, before any formal run

1. **`SetForceSolo` as a documented precondition of `AdvanceSimulation`** — one
   line, would have saved hours.
2. **Tool names in prose must match the registry** — the `W-NAME` event was our
   own doc lying. A CI check is proposed in
   [`BACKLOG.md`](BACKLOG.md) item 2.2.
3. **A worked `refPath` example everywhere an asset argument appears** — kills
   `W-SHAPE` outright.
4. **Point the read order at `Scripts/`** — the `W-DUP` events were all "prior art
   existed and was unreachable".
5. **Extend the `visual_loop_tool_notes` pattern per domain** — it is the only
   thing in the surface that actively prevents `W-DUP`, and it worked.

## 10. Success criteria for the tool surface

The surface is good enough when, across three models, blind:

- U0 and U1: `first_call_correct` ≥ 80%, zero `W-NAME`.
- U2: existing assets are found rather than duplicated in ≥ 2 of 3 runs.
- U3: ≥ 5 of 7 clauses `met` or `asked`, with the unit conversion correct, and a
  visual artifact delivered.
- **All scenarios: `false_success_claim` = 0.** This one is not a percentage
  target. One confident lie about a verified result costs more trust than ten
  honest failures.
