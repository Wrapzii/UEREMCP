# RB-16 — semantic environment coverage

**Owner:** WS-16. **Date:** 2026-07-30. **Status:** implementation evidence.

## Audit before build

- Epic PCG exposes 31 graph/spatial tools in the live registry, but no registered
  operation creates a complete seeded mountain/river/both-bank forest/rain world
  with a measured exclusion corridor. `[VERIFIED-RUNTIME: list_toolsets, UE 5.8,
  2026-07-30]`
- REAgentTools exposes `REDressWorkflowTools` scatter and
  `RECaptureWorkflowTools.capture_viewport_to_disk`, but the former does not solve
  terrain/Water/camera-follow coupling and the latter is retained as prior art for
  the general world capture hook. `[VERIFIED-RUNTIME: list_toolsets, UE 5.8,
  2026-07-30]`
- GeometryScripting and Water are enabled in `RE.uproject`.
  `[VERIFIED: $UEREMCP_LEGACY_PROJECT/RE.uproject:132-137]`

Disposition: preserve Epic PCG and RE capture primitives; add one goal-level
`BuildEnvironment` operation plus read-only inspect/validate support. Do not add
hundreds of placement/sculpt primitives or a second batch executor.

## UE 5.8 APIs used

- `ALandscape::Import` for deterministic heightmap-first terrain.
  `[VERIFIED: Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:1418-1420]`
- `ALandscapeProxy::GetHeightValues` for post-reload non-flat validation.
  `[VERIFIED: Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:1105]`
- `AWaterBodyRiver` and `AWaterBody::GetWaterSpline`.
  `[VERIFIED: Engine/Plugins/Experimental/Water/Source/Runtime/Public/WaterBodyRiverActor.h:28]`
  `[VERIFIED: Engine/Plugins/Experimental/Water/Source/Runtime/Public/WaterBodyActor.h:103]`
- `UWaterBodyComponent::bAffectsLandscape`; disabled because the generated
  heightmap is authoritative and synchronous WaterBrushManager creation hung the
  MCP game thread. `[VERIFIED: Engine/Plugins/Experimental/Water/Source/Runtime/Public/WaterBodyComponent.h:630]`
  `[VERIFIED-RUNTIME: BuildEnvironment WaterBrushManager hang, 2026-07-30]`
- `UEditorLoadingAndSavingUtils::{NewBlankMap,LoadMap,SaveMap}` for scratch-map
  persistence and reload validation. `[VERIFIED: Engine/Source/Editor/UnrealEd/Public/FileHelpers.h:45,64,75]`
- `UGameplayStatics::{GetPlayerPawn,GetPlayerCameraManager}` and
  `APlayerCameraManager::GetCameraLocation` for PIE weather following.
  `[VERIFIED: Engine/Source/Runtime/Engine/Classes/Kismet/GameplayStatics.h:201,219]`
  `[VERIFIED: Engine/Source/Runtime/Engine/Classes/Camera/PlayerCameraManager.h:705]`
- `USplineComponent` closest-location/tangent queries for measured both-bank and
  exclusion validation. `[VERIFIED: Engine/Source/Runtime/Engine/Classes/Components/SplineComponent.h:841,849]`

## Limitations

- Without `weather.rain_system_path`, rain rendering uses bounded instanced
  streaks. Camera/pawn following remains real and PIE-measured; the visual
  technology is reported as `approximated`.
- Screenshots are human-review evidence, never the structural success gate.
- PCG graphs remain available for bespoke authoring; WS-16 uses bounded HISMC
  scatter because the cross-domain river-distance constraint is solved inside the
  semantic operation.

## Live MountainRiverRain result

- Built `/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain` via
  ResolveIntent→BuildEnvironment (seed `4471`, revision `env:f49a66b5`).
  `[VERIFIED-RUNTIME: UEREMCP Environment MCP calls, 2026-07-30]`
- ValidateEnvironment (editor + PIE with `require_weather_follow_10m`) returned
  `no_change_required` with non-flat valley (~8315 cm), continuous river,
  both-bank forest (43/30), open channel, and `weather_followed_10m=true`.
- Screenshots: `tests/visual/mountain_river_rain/world_frame_00.png` and
  `world_frame_01.png`. Full call table:
  `tests/visual/MOUNTAIN_RIVER_RAIN_ACCEPTANCE.md`.
