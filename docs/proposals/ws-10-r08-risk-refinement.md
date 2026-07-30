# Proposal: R-08 risk refinement (WS-10 → WS-01)

- **From:** WS-10
- **To:** WS-01 (`docs/RISK_REGISTER.md`)
- **Date:** 2026-07-29
- **Evidence:** `docs/research/RB-09-animation-controlrig.md`

## Current text

> **R-08** — Control Rig / AnimBP state machines are effectively read-only via
> public API, so §9 cannot be delivered as described.

## Proposed refinement

Split the risk; severity stays Medium but likelihood drops for Control Rig:

| ID | Risk | Sev | Lik | Status |
|---|---|---|---|---|
| **R-08a** | AnimBP state-machine **authoring** is unsupported on agent surface (readable only). §9 locomotion authoring cannot be full graph replace. | Medium | **High** | open — mitigate with read + montage-based gameplay path |
| **R-08b** | Control Rig JSON round-trip / semantic fidelity unproven (authoring **does** exist via Epic `ControlRigTools`). | Medium | Medium | open — compose Epic tools; `fidelity` flags |

**Do not** keep "Control Rig is read-only" — contradicted by source + runtime
(`ControlRigTools.create`, `create_node`, `import_bones_from_asset` under
`/Game/__UeremcpTests/`).

§9 delivery strategy: montage/notify/socket + AnimBP **inspect**; not AnimBP SM
authoring; Control Rig via composed Epic tools when needed.

## Response (WS-01)

**Accepted in substance.** Kept a single **R-08** row (no R-08a/b IDs) with the
refined wording: AnimBP SM authoring unsupported; Control Rig authorable via
Epic; fidelity unproven. ADR-0004 open question closed from RB-09.
