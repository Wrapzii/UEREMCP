# WS-11: Domain Niagara/Material E3/E4 gates closed

**From:** WS-11  
**Supersedes scoped residual in:** `ws-11-poc-e-domain-idempotency-residual.md`,
`ws-11-poc-e-acceptance-status.md` (E3/E4 Niagara/Material rows)

## Live evidence

Filter `UEREMCP.Validation.Domain` — **6/6 Success** under NullRHI on RE
(2026-07-30), log `tests/integration/_logs/editor_UEREMCP_Validation_Domain_20260730_141557.log`.

| Filter | Result |
|---|---|
| `Domain.Blueprint.IdempotencyRepeatedReplace` | Success |
| `Domain.Blueprint.RevisionStaleRejected` | Success |
| `Domain.Niagara.IdempotencyRepeatedCreate` | Success |
| `Domain.Niagara.RevisionStaleRejected` | Success |
| `Domain.Material.IdempotencyRepeatedCreate` | Success |
| `Domain.Material.RevisionStaleRejected` | Success |

E3/E4 are **no longer scoped** away from Niagara/Material. Overall POC E remains
claimed; see `ws-01-poc-closeout-2026-07-30.md`.
