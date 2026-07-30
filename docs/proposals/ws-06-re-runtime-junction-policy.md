# WS-06: RE runtime junction policy

- **From:** WS-06
- **Date:** 2026-07-30
- **Status:** acknowledged

WS-06 development stays in `UEREMCP-ws06` / `ws-06-blueprint`. The RE project
`Plugins/UEREMCP` junction is orchestration-owned and must continue to point at the
orch plugin tree. WS-06 will not create, replace, or retarget that junction to ws06
or any other non-orch worktree.

Blueprint runtime verification will run only after orch merges the relevant WS-06
commits, through the orch junction. If a runtime check is needed before that merge,
WS-06 will hand it to WS-11/orch instead of changing the junction.
