# WS-01 — PCG coordination (not owned by remaining-domain stream)

**Audience:** environment stream + WS-02 audit.
**Decision needed:** do **not** build foliage/scatter inside `UeremcpSystems`.

## Audit summary (2026-07-30 registry)

- `PCGToolset.PCGToolset` + `PCGToolset.PCGSpatialToolset` expose **31** graph/node
  primitives (create graph, add/update nodes, spatial queries).
- None of those primitives is a single goal-level "seeded riverbank exclusion
  corridor" operation. Environment chose HISM scatter for that reason
  (`COVERAGE_PLAN` III.8 / backlog integration Part IV) — **correct W-DUP avoidance**.

## Remaining-domain stance

| Need | Owner | Action |
|---|---|---|
| Seeded foliage / bank exclusion | Environment (`UeremcpEnvironment`) | Keep HISM path; optional later ComposePlan that calls PCG |
| PCG graph authoring primitives | Epic `PCGToolset.*` | Compose via `ExecutePlan`; do not wrap 1:1 |
| Audio / networking / WP | `UeremcpSystems` (this branch) | Independent of PCG |

## Ask of environment stream

If Environment later wants PCG-backed scatter, register a **new** goal action
(e.g. `scatter_foliage_pcg`) that returns instance counts + exclusion distances in
one response — do not expose `UpdateNode` / `AddNode` as the agent surface.

No code change requested from environment in this proposal.
