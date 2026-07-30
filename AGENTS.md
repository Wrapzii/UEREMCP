# UEREMCP — Agent Operating Contract

You are one of many agents working this repository concurrently. This file is the
contract. Read it fully before your first edit. It is short on purpose.

## What this project is

A goal-level Unreal Engine 5.8 agent interface: an AI describes a complete outcome
("create a replicated fireball ability with VFX, materials, tags, and damage"),
submits one structured JSON request, and gets back one validated structured result
with a change manifest — instead of issuing hundreds of primitive editor calls.

Target project: `$UEREMCP_LEGACY_PROJECT` (UE 5.8).
Existing prior art: `REAgentTools`.

## What this project is NOT

If your output looks like any of these, you have failed:

- a renamed wrapper over Epic's existing toolsets
- hundreds of low-level node/pin tools exposed to the agent
- a thin wrapper around arbitrary Python execution
- screenshot-driven automation
- anything that reports success without compiling and validating
- a system that cannot return complete graph structure, or cannot batch
- a pile of disconnected research documents with no executable proof of concept
- an architecture document with no proof of concept
- anything that can silently destroy user assets

## Read order — do not skip

0. **`docs/WHY.md`** — the actual problem, and the cost model. Short. Read it first:
   it contains the token-economics math that determines whether your design decisions
   help or hurt, and it is counterintuitive.
1. **`docs/GROUNDED_FACTS.md`** — verified UE 5.8 API surface. Read before designing
   anything. Cite it rather than re-deriving it.
2. **`docs/adr/`** — accepted decisions. These are frozen. See "Frozen decisions" below.
3. **`docs/WORK_ALLOCATION.md`** — find your workstream, your owned paths, your deliverables.
4. **`docs/RESEARCH_PROTOCOL.md`** — how to cite sources and tag confidence.
5. **`schemas/`** — the shared JSON contracts. Do not fork them.
6. Your own brief in `docs/research/RB-*.md`.

## The eight hard rules

### 1. Never claim an Unreal API exists without reading it

The dominant failure mode on this project is a confident assertion about an Unreal
API that does not exist, or does not exist in 5.8, or is editor-only, or is private.
Every API claim in a deliverable carries a verification tag:

- `[VERIFIED: <path>:<line>]` — you read it in engine source or a header on this machine
- `[VERIFIED-RUNTIME: <how>]` — you executed it in the editor and observed the result
- `[DOCS: <url>]` — official Epic documentation, no local confirmation
- `[UNVERIFIED]` — inference, forum, or memory. **Allowed in research notes. Never
  allowed in an ADR, a schema, or an implementation comment.**

An untagged API claim is treated as `[UNVERIFIED]` and will be rejected by the critic
workstream. "I recall that `UNiagaraSystem` has..." is not evidence. Go read it.

### 2. Audit before you build

Epic ships 27 domain toolsets (`GROUNDED_FACTS.md §3`) including Niagara, GAS,
gameplay tags, PCG, StateTree, UMG, physics, animation, and automation testing.
REAgentTools ships 15 more. Before proposing any new primitive, record in
`docs/audit/` what the existing equivalent does and why it is insufficient.

Adding a tool that duplicates a working Epic tool is a defect, not progress.

### 3. Stay inside your owned paths

`docs/WORK_ALLOCATION.md` assigns every path to exactly one workstream. Editing a
path you do not own will collide with another agent working simultaneously.

To change something you do not own: write a proposal to
`docs/proposals/<your-ws>-<topic>.md` and keep working on what you do own. Do not
block, and do not edit it anyway.

Shared files (`schemas/**`, `docs/adr/**`, `docs/GROUNDED_FACTS.md`,
`AGENTS.md`, this list) are owned by **WS-01 only**.

### 4. Frozen decisions are frozen

Any ADR with `Status: Accepted` is settled. Do not redesign it, do not "improve" it
in passing, do not implement an alternative. If you have evidence it is wrong, write
`docs/proposals/<your-ws>-adr-<n>-challenge.md` with the evidence and continue with
the accepted design in the meantime.

This rule exists because a protocol that fourteen agents each improved slightly is
not a protocol.

### 5. One semantic operation, not many primitive calls

The whole point. Before adding a tool, ask: does this force the agent into an
inspect → mutate → inspect loop? If yes, the design is wrong. Batch it, or express
it as a complete-state submission.

Corollary from the cost model in `docs/WHY.md`: **agent cost is superlinear in call
count and only linear in payload size.** When choosing between returning more
information now and letting the agent ask for it later, return it now. The math is
not close. Do not "optimise" a response by trimming useful context.

Primitives may exist internally. They should not be the normal agent-facing surface
— use `FToolset::SetNameFilters` to hide them (`GROUNDED_FACTS.md §2.2`).

### 6. Success requires verification, not tool-call completion

"The tool returned OK" is not success. A creation or modification is only successful
when you have re-read the result and confirmed it. For graphs that means: nodes
exist, required pins are connected, execution paths are connected, compilation
succeeded, the asset saved, dependencies resolve.

Report honest statuses: `created_and_validated`, `modified_and_validated`,
`created_with_warnings`, `failed_validation`, `rolled_back`, `partially_completed`.
Never report success for partial work. If you could not verify, say so.

### 7. Every deliverable ships with tests

Implementation without tests is not done. Unit tests for schema/serialization/patch
logic; editor integration tests for anything touching assets. See
`docs/POC_ACCEPTANCE.md` for the acceptance bar.

### 8. Never destroy user content

Destructive operations default to `dry_run: true`. Preserve user-created content
unless replacement was explicitly requested. Deleting and recreating a *graph* to get
determinism is fine and encouraged; deleting a user's *asset* because it was in the
way is not.

## Frozen decisions (as of 2026-07-30)

Full text in `docs/adr/`. Summary so you do not re-litigate:

| ADR | Decision |
|---|---|
| 0001 | Baseline is UE 5.8; Epic's `ModelContextProtocol` + `ToolsetRegistry` are the substrate |
| 0002 | We host as an in-process C++ editor plugin registering `AICallable` toolsets — **not** a new external MCP server |
| 0003 | One versioned JSON request envelope in, one JSON response envelope out |
| 0004 | Graphs are exchanged as complete structured JSON with stable IDs, content hash, and revision |
| 0005 | Rollback builds on `FileSandbox` + editor transactions; not a bespoke mechanism |
| 0006 | Idempotency via stable paths + idempotency keys + `expected_revision` conflict checks |
| 0007 | C++ primary for agent-facing toolsets and domain services that touch graphs/assets/compilation; Python exploratory only |
| 0008 | Templates are JSON + `UeremcpTemplates` + `execute_plan`; do **not** subclass `UAgentSkill`; elemental variation via inputs/modifiers |
| 0009 | Long jobs: inline when `timeout_ms == 0`; else `partially_completed` + poll `get_job_result`; UEREMCP-owned cancel/progress |
| 0010 | Application-layer security in `UeremcpSecurity` — permission tiers, path validator, mutator queue, audit; do not fork Epic MCP for tokens |

ADR-0011 (`asset_state` for non-graph assets) remains **Proposed**, not Accepted.

## Definition of done

A deliverable is done when all of these hold:

- [ ] Every API claim carries a verification tag (rule 1)
- [ ] The Epic/REAgentTools equivalent is audited in `docs/audit/` (rule 2)
- [ ] Only owned paths were modified (rule 3)
- [ ] Conforms to the frozen schemas in `schemas/` — validated with `tools/validate_schemas.py`
- [ ] Tests exist and pass, or the reason they cannot run is stated explicitly
- [ ] Open questions and limitations are written down, not omitted
- [ ] The handoff artifact named in `docs/WORK_ALLOCATION.md` exists

Partial completion is acceptable and expected. **Silent** partial completion is not.
State plainly what you finished, what you did not, and why.

## Git conventions for concurrent agents

- Branch per workstream: `ws-<nn>-<slug>` (e.g. `ws-07-niagara`).
- Never commit directly to `main` or `master`.
- Commit message first line: `[WS-nn] <imperative summary>`.
- Rebase onto `main` before opening a PR; if you hit a conflict in a path you do not
  own, that is rule 3 being violated somewhere — report it, do not resolve it by
  overwriting.
- Do not commit `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, or
  `__pycache__/`.

## Escalation

Stop and write to `docs/proposals/` rather than guessing when:

- an accepted ADR appears to be contradicted by engine reality
- two workstreams need the same schema change
- a required Unreal API turns out not to exist and the workaround changes the architecture
- an operation cannot be made verifiable

Recording a blocker precisely is more valuable than working around it silently.
