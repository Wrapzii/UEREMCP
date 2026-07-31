# WS-01 remaining-domain coverage closeout

**Branch:** `ws-01-remaining-domain-coverage`  
**Base:** local main tip `d9f4936` (GitHub `origin/main` remains sparse Phase-0; local deploy/main is the integrated tip used by concurrent streams)  
**Not pushed. Not merged. RE junction not retargeted.**

## Inventory (COVERAGE_PLAN thin domains, non-environment)

| Item | Choice | Result |
|---|---|---|
| Audio | Implement SoundCue goal ops; block MetaSound graph | `create_audio_cue`, `inspect_audio` |
| Networking | Goal audit (not thin wrap of get/set_variable_replication) | `validate_replication` |
| World Partition | Inspect + dry-run-default repair; block HLOD commandlets | `inspect_world_partition`, `repair_world_partition` |
| PCG (non-env) | Proposal only | `docs/proposals/ws-01-pcg-coordination.md` |

## Commits / verification

- Schemas: `python tools/validate_schemas.py` → OK (36)
- Schema unit tests: 3/3 pass
- Ownership: clean except intentional `UEREMCP.uplugin` module line (WS-03) — see `ws-01-systems-module-uplugin.md`
- BuildPlugin: **blocked** by concurrent UBT `ConflictingInstance` (do not interrupt env live work)
- Live MCP: handoff in `ws-01-systems-live-handoff.md` (separate editor; no RE retarget)

## Ledger updates

- `docs/BACKLOG.md` 4.audio / 4.net / 4.wp → `completed_with_documented_limitation`
- `docs/COVERAGE_PLAN.md` Part IV CP-audio/net/wp updated (WP no longer `blocked_external`)
- `docs/CAPABILITY_CATALOG.md` rows added; `configure_replication` marked superseded by `validate_replication`
