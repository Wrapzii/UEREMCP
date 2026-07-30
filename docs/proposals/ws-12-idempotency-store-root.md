# WS-12 → WS-05: idempotency + audit store under Saved/UEREMCP

- **From:** WS-12
- **To:** WS-05 (idempotency store location per ADR-0006 open question)
- **Date:** 2026-07-29
- **Related:** ADR-0005, ADR-0006, RB-13, WS-11 sandbox proposal

## Ask

Place the durable idempotency map (and accept WS-12 audit JSONL alongside) under:

```
<ProjectSavedDir>/UEREMCP/idempotency/
<ProjectSavedDir>/UEREMCP/audit/
```

Not under `Intermediate/Sandboxes/`.

## Why

`ISandboxInstance` tracks **content mount points only**; `Saved/` and `Config/` are
**not** sandboxed `[VERIFIED: ISandboxInstance.h:28-30]`. A failed batch that
`Discard()`s must not erase the `(idempotency_key → response)` record or the audit
trail that explains what was attempted.

In-memory session store remains the Wave 1 minimum (ADR-0006). Disk under
`Saved/UEREMCP/` is the preferred restart-surviving location for Wave 2.

## Security note

`Saved/` is writable and outside rollback — path policy must still confine writes to
`Saved/UEREMCP/**` (not arbitrary Saved trees). WS-12 path validator will treat that
prefix as an allowed filesystem root for plugin-owned metadata only.
