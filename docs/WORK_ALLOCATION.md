# Work Allocation and File Ownership

**Owner:** WS-01. **Read before your first edit.**

Every path in this repository belongs to exactly one workstream. This is the
mechanism that lets many agents work the repo simultaneously without destroying each
other's work. It is not bureaucracy — with 15 concurrent agents and no ownership map,
the shared files (schemas, ADRs, the plugin build) become a merge war within an hour.

## The rule

**Edit only paths your workstream owns.**

To change something you do not own: write `docs/proposals/<your-ws>-<topic>.md`
stating what you need and why, then keep working on what you do own. Do not block on
it. Do not edit it anyway. The owning workstream picks it up.

Unowned path? It is WS-01's until assigned. Propose, do not squat.

## Workstreams

| WS | Role | Owns | Wave |
|---|---|---|---|
| **WS-01** | Lead Architect | `docs/adr/**`, `docs/GROUNDED_FACTS.md`, `docs/WHY.md`, `docs/WORK_ALLOCATION.md`, `docs/RESEARCH_PROTOCOL.md`, `docs/ROADMAP.md`, `docs/RISK_REGISTER.md`, `docs/POC_ACCEPTANCE.md`, `docs/CAPABILITY_CATALOG.md`, `docs/SWARM_LAUNCH.md`, `docs/BACKLOG.md`, `docs/COVERAGE_PLAN.md`, `Scripts/**`, `schemas/common/**`, `schemas/envelope/**`, `schemas/graph/**`, `schemas/template-library/**`, `schemas/domains/environment/**`, `schemas/domains/audio/**`, `schemas/domains/networking/**`, `schemas/domains/world_partition/**`, `Plugins/UEREMCP/Source/UeremcpEnvironment/**`, `Plugins/UEREMCP/Source/UeremcpSystems/**`, `AGENTS.md`, `README.md` | 0 |
| **WS-02** | Existing-System Auditor | `docs/audit/**` | 1 |
| **WS-03** | Unreal Plugin Architect | `Plugins/UEREMCP/UEREMCP.uplugin`, `Plugins/UEREMCP/Source/UeremcpCore/**` | 1 |
| **WS-04** | MCP Server / Transport | `Plugins/UEREMCP/Source/UeremcpTransport/**`, `docs/research/RB-04-*.md` | 1 |
| **WS-05** | JSON & Protocol Architect | `Plugins/UEREMCP/Source/UeremcpProtocol/**`, `schemas/batch/**`, `schemas/domains/_shared/**`, `tools/validate_schemas.py` | 1 |
| **WS-06** | Blueprint Specialist | `Plugins/UEREMCP/Source/UeremcpBlueprint/**`, `schemas/domains/blueprints/**` | 2 |
| **WS-07** | Niagara Specialist | `Plugins/UEREMCP/Source/UeremcpNiagara/**`, `schemas/domains/niagara/**` | 2 |
| **WS-08** | Material & VFX Assets | `Plugins/UEREMCP/Source/UeremcpMaterial/**`, `Plugins/UEREMCP/Resources/Materials/**`, `schemas/domains/materials/**` | 2 |
| **WS-09** | Gameplay & GAS | `Plugins/UEREMCP/Source/UeremcpGameplay/**`, `schemas/domains/gameplay/**` | 3 |
| **WS-10** | Animation & Control Rig | `Plugins/UEREMCP/Source/UeremcpAnimation/**`, `schemas/domains/animation/**` | 3 |
| **WS-11** | Validation & Testing | `Plugins/UEREMCP/Source/UeremcpValidation/**`, `tests/**` | 1 |
| **WS-12** | Security & Reliability | `Plugins/UEREMCP/Source/UeremcpSecurity/**`, `docs/SECURITY.md` | 2 |
| **WS-13** | Documentation | `docs/guide/**` | 3 |
| **WS-14** | Integration Critic | `docs/reviews/**` | continuous |
| **WS-15** | Template & Pattern Library | `Plugins/UEREMCP/Source/UeremcpTemplates/**`, `templates/**`, `schemas/domains/templates/**` | 2 |

Everyone additionally owns `docs/research/RB-<their-number>-*.md` and may write freely
to `docs/proposals/<their-ws>-*.md`.

## Waves

Waves are dependency order, not a schedule. Later waves may start research
immediately; they may not start *implementation* until their blockers land.

**Wave 0 — done.** Grounded facts, ADRs 0001–0006, core schemas, this document.
Frozen. See `AGENTS.md` "Frozen decisions."

**Wave 1 — unblocks everything. Start here.**

| WS | First deliverable | Why it blocks others |
|---|---|---|
| WS-02 | `docs/audit/epic-toolsets.md` — actual tool inventory of the 27 Epic toolsets | Every domain WS needs to know what already exists before building. **The single highest-leverage first task in the project.** |
| WS-03 | A compiling `UToolsetDefinition` with one `AICallable` tool reachable from an MCP client | Proves ADR-0002. If this fails, the architecture is wrong and everyone needs to know on day one, not week three. |
| WS-05 | `tools/validate_schemas.py` green; envelope round-trip in C++ | Domain WSs cannot write conformant tools without it. |
| WS-04 | `RB-04` findings: stdio vs HTTP, progress, cancellation, auth | Determines whether long-running jobs (§18) are feasible as designed. |
| WS-11 | Editor integration test harness that can run one test | Nothing can be verified without it, and "verified" is the project's whole premise. |

**Wave 2 — the core thesis.**

| WS | First deliverable |
|---|---|
| WS-06 | POC A: Blueprint complete round-trip. **The project's make-or-break deliverable.** |
| WS-07 | Niagara inspection to `graph.schema.json`, then POC B construction |
| WS-08 | Semantic VFX material creation; procedural texture generation |
| WS-15 | Template store, search, instantiate, promote |
| WS-12 | Permission tiers, allowed roots, audit log |

**Wave 3 — breadth.** WS-09 (GAS, POC D), WS-10 (animation/Control Rig), WS-13 (docs).

**WS-14 runs continuously from Wave 1.** It reviews others' output, does not produce
implementation, and writes only to `docs/reviews/`.

## Handoff artifacts

A workstream is not done when its code compiles. It is done when the artifact the
next workstream needs exists.

| From | To | Artifact |
|---|---|---|
| WS-02 | all domain WS | `docs/audit/` capability matrix with preserve/replace/improve disposition per tool |
| WS-03 | all domain WS | `UeremcpCore` — toolset base class, service registration, main-thread dispatch |
| WS-05 | all domain WS | `UeremcpProtocol` — envelope parse/serialise, validation, `$ref` resolution, revision hashing |
| WS-04 | WS-05 | job model constraints from the transport layer |
| WS-11 | all | test harness + `Rollback.MultiAssetDiscard`, which gates every rollback claim |
| domain WS | WS-15 | patterns worth promoting to templates |
| domain WS | WS-14 | anything claiming to be complete |

## Cross-cutting obligations

Regardless of workstream:

1. **Research before building.** Your `docs/research/RB-*.md` brief lands before your
   implementation, with verification tags per `AGENTS.md` rule 1.
2. **Audit before adding.** Epic ships 27 toolsets. Record the existing equivalent in
   `docs/audit/` and why it is insufficient — WS-02 owns the file, you supply your
   domain's rows via proposal.
3. **Tests ship with code.** No exceptions. See `docs/POC_ACCEPTANCE.md`.
4. **Register your actions.** Every agent-facing `action` you add gets an entry in
   `docs/CAPABILITY_CATALOG.md` (via proposal to WS-01) and a `specification` schema
   under `schemas/domains/<your-domain>/`.
5. **Declare your limitations.** Use `capability_notes` in responses and a
   "Limitations" section in your docs. An undocumented limitation is a defect
   (`AGENTS.md` rule 6).

## If you are the only agent running

Work the waves in order, one workstream at a time, and keep the ownership discipline
anyway — it keeps the deliverables separable and the reasoning traceable. Wave 1
WS-03 first: if `UToolsetDefinition` with an envelope-shaped `AICallable` tool does
not work, every other decision changes.

## Enforcement

`tools/check_ownership.py` maps changed paths to workstreams. Run it before you
commit:

```bash
python tools/check_ownership.py --ws WS-07
```

It is advisory, not a lock. It exists to catch the accident, not to stop the
determined. If it flags you, you are probably about to overwrite another agent's
work.
