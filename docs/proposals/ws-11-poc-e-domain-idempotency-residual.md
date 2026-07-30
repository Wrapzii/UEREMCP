# WS-11 residual: domain idempotency / revision beyond Blueprint

**From:** WS-11  
**To:** WS-07 (Niagara), WS-08 (Material), WS-05 (protocol store persistence)  
**Date:** 2026-07-30  
**Context:** POC E E3/E4

## Proven on this tip

| Layer | Filter / path | Result |
|---|---|---|
| Protocol + CurveFloat scratch | `UEREMCP.Validation.Idempotency.RepeatedCreate` | PASS (prior 6/6 shipping log) |
| Protocol + CurveFloat scratch | `UEREMCP.Validation.Revision.StaleRejected` | PASS (prior 6/6 shipping log) |
| Blueprint `submit_graph` | `UEREMCP.Validation.Domain.Blueprint.IdempotencyRepeatedReplace` | Gate added (this branch) |
| Blueprint `submit_graph` | `UEREMCP.Validation.Domain.Blueprint.RevisionStaleRejected` | Gate added (this branch) |

Blueprint already implements `expected_revision` reject and `no_change_required` for identical replace
`[VERIFIED: UeremcpBlueprintToolset.cpp expected_revision / no_change paths]`.

## Not proven (precise residual)

1. **Niagara** — no `expected_revision` / idempotency_key handling found under
   `UeremcpNiagara/**` for create/replace. E3/E4 cannot be claimed on the Niagara
   pipeline until WS-07 wires stable-path + revision guards and WS-11 adds a
   Validation domain gate analogous to Blueprint.
2. **Material** — Material toolset has honesty for `validate=false` but not an
   ADR-0006 revision/idempotency gate exercised by Validation for
   `create_vfx_material` / procedural textures.
3. **Idempotency across editor restart** — `FUeremcpIdempotencyStore` documents
   disk hydrate intent; Validation 6/6 residuals explicitly exclude persistent
   idempotency across restarts. Prefer WS-05 confirmation + a Validation restart
   pair before claiming.

## Ask

- WS-07 / WS-08: adopt the same `expected_revision` + stable path +
  `no_change_required` patterns as Blueprint, or document why the domain is
  exempt.
- WS-11 will add `UEREMCP.Validation.Domain.Niagara.*` / `Material.*` gates once
  the domain APIs exist; until then E3/E4 remain **Blueprint + protocol** scoped
  in the POC E bundle.
