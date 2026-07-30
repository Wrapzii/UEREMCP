# WS-14 proposal: Wire Security policy onto first mutate path

- **From:** WS-14
- **To:** WS-12 (+ first mutating domain WS)
- **Date:** 2026-07-30
- **Blocks:** Treating `UeremcpSecurity` as an enforcement gate
- **Review:** `docs/reviews/wave-2-2026-07-30.md` H-4

## Problem

Wave 2 landed a solid policy library:

- Destructive dry-run force when not explicit (`UeremcpPermissionPolicy.cpp:102–108`)
- Path policy + unit tests + `docs/SECURITY.md`

No domain toolset calls `FUeremcpPermissionPolicy::Evaluate` or `FUeremcpPathPolicy`
before mutation. `FUeremcpMutatorQueue` remains an intentional stub
(`IsImplemented() == false`).

R-07 correctly stays `open`. Residual text that implies Wave 2 security is still
entirely ahead-of-work understates the library while any “security landed” summary
overstates enforcement.

## Ask

1. Pick the first mutating agent path (likely Blueprint `submit_graph` or Material
   `create_vfx_material` once implemented) and call path + permission policy before work.
2. Keep MutatorQueue stub until designed; do not pretend `TryAcquire` succeeds.
3. Update R-07 residual to “library+tests landed; mutate-path wiring open.”
