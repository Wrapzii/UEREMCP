# RB-07: Niagara system, emitter, and module-stack read/write

- **Owner:** WS-07
- **Status:** not_started
- **Blocks:** POC B, POC C, ADR-0004 confidence for non-Blueprint graphs
- **Priority:** high

## Framing

Niagara is the domain the owner most wants ("generating effects and templates from the
beginning" — `docs/WHY.md`) and the domain where the shared graph representation is
most likely to fit badly. A Niagara system is not one node graph: it is a system
script, a set of emitters, per-emitter module *stacks* across execution phases
(Emitter Spawn/Update, Particle Spawn/Update, Event handlers), renderers, and a
parameter store — with compiled scripts underneath.

`ADR-0004` anticipates this: `graph_type` includes `NiagaraSystemGraph`,
`NiagaraEmitterGraph`, `NiagaraModuleStack`, and `NiagaraScriptGraph`, and
`extensions.niagara` is yours to define. **If module stacks genuinely cannot be
expressed as `nodes`/`links`, say so with evidence and challenge ADR-0004** — that is
a legitimate and expected outcome, not a failure.

Read `$RAT/Docs/NIAGARA_BATCHING.md` first. It documents a working batching pattern
against Epic's Niagara tools and will save you days.

## Questions

### A. Reading

1. What public/editor API enumerates a `UNiagaraSystem`'s emitters, and each
   `UNiagaraEmitter`'s module stack per execution phase? Investigate
   `UNiagaraStackEntry`/`UNiagaraStackViewModel` (editor-only view-model territory —
   determine if it is the *only* path, which would be a significant finding) versus
   direct script/graph access via `UNiagaraScript` and `UNiagaraGraph`.
2. How are module *inputs* read — the values set on a stack entry, including
   dynamic-input sub-graphs and linked parameters?
3. How is the parameter store read — user, system, emitter, particle parameters, with
   types and defaults? `UNiagaraUserRedirectionParameterStore` for user params.
4. How are renderers read (sprite, mesh, ribbon, light, decal) with their bindings and
   material assignments?
5. How are data interfaces, curves, and mesh/skeletal-mesh sampling set-ups read?
6. Emitter-level settings: sim target (CPU/GPU), determinism, local space, bounds mode,
   fixed bounds, warmup, scalability/LOD, spawn behaviour.
7. What does compilation status look like, and how do compile errors map back to a
   specific module/stack entry so `diagnostic.node_id` can be filled?

### B. Writing

8. Can emitters be added from an emitter *template* or parent emitter asset
   programmatically, and does inheritance (`bIsInheritable`, parent overrides) survive?
9. Can module stack entries be added, removed, reordered, and have inputs set
   programmatically? Which API — editor view-model, or a data-level API?
10. Can a *new module script* be authored, or are we restricted to composing existing
    modules? This bounds how much genuinely novel effect construction is possible
    (master prompt §5.4).
11. Is there a reliable way to create a complete system from scratch versus duplicating
    a template asset and modifying it? **Be honest about which is actually reliable** —
    ADR-0004 explicitly permits duplicate-and-modify where it is more deterministic.
12. How is compilation triggered and awaited? Niagara compiles asynchronously — how do
    we know when it is genuinely done, and not report success mid-compile?
13. What does "validated" mean for a Niagara system beyond compiling? Candidates:
    renderers bound to valid materials, emitters non-empty, required attributes written
    before read, bounds sane, no missing data interfaces.

### C. Semantic layer

14. Design `specification` for `create_niagara_effect` covering the master prompt's
    effect categories (projectile, explosion, beam, lightning, aura, shield, magic
    circle, sword trail, impact, portal, teleport, weather, footstep, ...) such that
    **new categories are additive** and do not require redesign.
15. What is the minimum set of reusable emitter archetypes (core, shell, sparks, smoke,
    ribbon trail, impact burst) that composes most of those categories?
16. What must `extensions.niagara` carry that `nodes`/`links` cannot express?

### D. Verification

17. Can a system be previewed/simulated headlessly to confirm particles actually spawn,
    or is PIE required? A "runtime smoke test" that cannot run cheaply will not run.
18. What performance signals are readable — particle counts, bounds, GPU/CPU cost?

## Deliverables

- [ ] Complete read of an existing project Niagara system into `graph.schema.json` +
      `extensions.niagara` — `[VERIFIED-RUNTIME]`
- [ ] POC B: `create_niagara_effect` for the fireball spec in `docs/POC_ACCEPTANCE.md`
- [ ] POC C: ice variation from the fireball, one request
- [ ] `schemas/domains/niagara/` specification schemas
- [ ] An explicit verdict on ADR-0004 fit for module stacks, with evidence
- [ ] Emitter archetype list handed to WS-15 for the template library
