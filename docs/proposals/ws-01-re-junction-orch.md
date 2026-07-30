# WS-01: RE project plugin junction retarget (orch)

**Date:** 2026-07-30  
**Status:** Applied

## Change

`$UEREMCP_LEGACY_PROJECT\Plugins\UEREMCP` was a directory
junction pointing at `UEREMCP-ws03\Plugins\UEREMCP` (6-module uplugin; no
`UeremcpBlueprint` sources). Retargeted to:

`$UEREMCP_ROOT-ws01\Plugins\UEREMCP` (`ws-01-orch`)

## Rationale

Orch carries all eight Wave 2 module sources (including `UeremcpBlueprint`) and the
full eight-entry `Modules[]` in `UEREMCP.uplugin` (from WS-03 `223eed7` / orch
history). WS-03 tip `8faabf7` restores the same uplugin list; the ws03 worktree
still lacks Blueprint sources until WS-06 lands there.

## Verification

- Junction target: `UEREMCP-ws01\Plugins\UEREMCP`
- `UEREMCP.uplugin` lists: Protocol, Blueprint, Niagara, Security, Templates, Core,
  Transport, Validation
- `Source/UeremcpBlueprint` present under junction target

## Policy (integration tip)

**Do not retarget** `RE\Plugins\UEREMCP` to `UEREMCP-ws03` (or any non-orch worktree).
`UEREMCP-ws01` / branch `ws-01-orch` is the integration tip; only that tree is
guaranteed to carry all eight module sources including `UeremcpBlueprint`.

