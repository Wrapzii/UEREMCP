# WS-11 → WS-01: D5 multi-client proof + B10 visual residual closeout

**Owner:** WS-11  
**Branch / worktree:** `ws-11-multiplayer-visual-hardening` @
`UEREMCP-ws11-multiplayer-visual-hardening`  
**Date:** 2026-07-30  
**Requests:** update `docs/POC_ACCEPTANCE.md` D5 note, `docs/CAPABILITY_CATALOG.md`
networking proof language, and `docs/proposals/ws-01-poc-closeout-2026-07-30.md`
residuals table. WS-11 does not edit those WS-01 paths.

## Verdict

| Residual | Status | Evidence |
|---|---|---|
| **D5 genuine multi-client** | **CLOSED (live)** | listen-server + 2 remote clients; client cast → server stamina + damage; second client observes Multicast cosmetics (Niagara + cast montage) and replicated health |
| **B10 warm-pixel / visible render** | **CLOSED (live, rendered)** | `UEREMCP.Niagara.POCB.VisibleRender` PASS with warm pixels + particle counts; PNG supplementary only |

## D5 live evidence

- Filter: `UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE`
- Runner: `tests/run_d5_multiclient.ps1` (`-WithRendering`, no screenshot proof)
- Artifact: `tests/integration/_artifacts/d5_pattern_b_multiclient.json`
- Log (reconfirm): `tests/integration/_logs/editor_UEREMCP_Validation_Gameplay_PatternB_MultiClientPIE_20260730_150518.log`
- Prior green log: `tests/integration/_logs/editor_UEREMCP_Validation_Gameplay_PatternB_MultiClientPIE_20260730_150404.log`

Exact artifact fields `[VERIFIED-RUNTIME: d5_pattern_b_multiclient.json]`:

```json
{
  "status": "pass",
  "remote_clients": 2,
  "pie_worlds": 3,
  "client_worlds": 2,
  "server_connections": 2,
  "client_intent_issued": true,
  "server_authority_accepted": true,
  "owner_observed_replicated_stamina": true,
  "second_client_observed_cast_effect": true,
  "second_client_observed_cast_niagara": true,
  "second_client_observed_cast_montage": true,
  "expected_effect_path_count": 5,
  "cast_attempts": 1,
  "server_applied_damage": true,
  "second_client_observed_replicated_damage": true,
  "initial_server_stamina": 107,
  "min_server_stamina": 95,
  "initial_server_target_health": 116,
  "min_server_target_health": 70,
  "max_observer_effects": 3
}
```

Log markers: `UEREMCP_D5_OUTCOME=PASS`, `Result={Success}`,
`[REAbility] cast fire_s ...`.

### What the harness proves

1. PIE listen-server under one process with `PlayNumberOfClients = 3`
   (server + 2 remote clients) `[VERIFIED: PIENetworkComponent.cpp:104-119]`.
2. Non-authority client `CastAbility("fire_s")` →
   `Server_RequestCastAbility` → `AuthorityCastAbility`
   `[VERIFIED: REPlayerVisualCombatComponent.cpp:3963-3984]`.
3. Server stamina spend (`107 → 95`) with regen disabled for the sample window.
4. Owning client observes replicated stamina (`min_owner_client_stamina=95`).
5. Server projectile damage via Visibility-blocking target capsule
   `[VERIFIED: REPlayerVisualCombatComponent.cpp:4409-4412]` (`116 → 70`).
6. Second client observes replicated target health (`70`).
7. Second client observes Pattern B Multicast cosmetics:
   - Niagara systems from `DA_VFX_*` / legacy soft paths (`max_observer_effects=3`)
   - Cast montage on the simulated caster replica
   `[VERIFIED: RECharacter.cpp:391-399; REPlayerVisualCombatComponent.cpp:4136-4156]`.

### Limitations (honest)

- Bootstrap uses `FAutomationEditorCommonUtils::CreateNewMap()` +
  `BP_REGameMode` because `/Game/RE/Hub/Lvl_Hub.umap` is missing on this machine
  `[VERIFIED-RUNTIME: Content/RE/Hub has no Lvl_Hub.umap]`.
- Target capsule Visibility response is a test-only runtime mutation so the
  projectile Visibility sweep can hit a pawn (production RE still uses
  `ECC_Visibility` for bolts).
- `Multicast_AbilityCosmetics` is Unreliable; harness retries up to 3 casts if
  cosmetics miss after gameplay evidence lands. Live pass used `cast_attempts=1`.
- Runner uses `-WithRendering` so Niagara components spawn; montage proof remains
  available under NullRHI.

## B10 live evidence

- Filter: `UEREMCP.Niagara.POCB.VisibleRender`
- Runner: `tests/run_poc_b10_visible_render.ps1`
- Artifact: `tests/integration/_artifacts/poc_b10_fireball.png`
- Log (regression): `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_150551.log`
- Prior green log: `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_VisibleRender_20260730_142913.log`

Exact markers `[VERIFIED-RUNTIME: VisibleRender 20260730_150551]`:

- `warm_changed_pixels=36921`
- `changed_pixels=46539`
- `particle_count=412`
- `total_spawned_particles=705`
- `runtime_emitter_instances=6`
- `UEREMCP_POC_B10_OUTCOME=PASS`

PNG is supplementary only; the programmatic warm-pixel / particle gate is the
PASS criterion.

## Proposed WS-01 doc edits

1. **`docs/POC_ACCEPTANCE.md` D5 row** — keep static checklist as minimum; add that
   WS-11 now ships a live multi-client harness
   (`UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE`) with machine JSON
   evidence, not a silent skip.
2. **`docs/CAPABILITY_CATALOG.md`** — networking / Pattern B: mark optional
   multi-client proof as available via `tests/run_d5_multiclient.ps1`.
3. **Closeout residual table** — move D5 multi-client and B10 warm-pixel rows from
   open residuals to closed-with-evidence, citing the artifact paths above.

## Owned deliverables (this branch)

- `Plugins/UEREMCP/Source/UeremcpValidation/Private/Tests/GameplayPatternBMultiClient.spec.cpp`
- `Plugins/UEREMCP/Source/UeremcpValidation/UeremcpValidation.Build.cs` (`RE`, `LevelEditor`)
- `tests/run_d5_multiclient.ps1`
- `tests/unit/test_d5_multiclient_harness.py`
- `tests/run_editor_tests.ps1` (surfaces `UEREMCP_D5_*` markers)
- `tests/README.md`
- this proposal
