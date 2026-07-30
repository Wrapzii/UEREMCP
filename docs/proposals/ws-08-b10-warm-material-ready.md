# WS-08 B10 warm-material generator ready for orch

Status: Material source and editor assertions are ready; orch DLL/recreate/B10 runtime
proof remains to be run after cherry-pick.

## Diagnosed root cause

The generated master was already `BLEND_Additive`, two-sided, and `MSM_Unlit`, with
`MP_EmissiveColor` connected. The black result came from the compiled expression path:

1. `dynamic_color` only blended two material-instance vector parameters. It did not use
   `UMaterialExpressionParticleColor`, so Niagara `Particles.Color` never reached
   emissive or opacity. `UMaterialExpressionParticleColor::Compile` emits the renderer
   `ParticleColor` external value, with named RGB/A outputs.
   [VERIFIED: Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressions.cpp:10069-10088]
   [VERIFIED: Engine/Source/Runtime/Engine/Private/Materials/HLSLMaterialTranslator.cpp:6027-6031]
2. The camera-facing sprite core path multiplied emissive directly by Fresnel. A zero
   Fresnel result therefore zeroed the otherwise warm MI tint. The generated animated
   noise and optional main-texture modulation had the same zero-multiplication failure
   mode. [VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp]
3. Opacity was connected only for selected trail features, not for all generated
   additive masters, and it did not consume renderer particle alpha.
   [VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp]
4. No `UMaterialExpressionDynamicParameter` was present. It is not needed for this B10
   fix because the Niagara scaffold writes `Particles.Color`; the corrected master now
   consumes that renderer value directly. [VERIFIED: Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp]

## Generator correction

- Multiply the warm MI element tint by Niagara Particle Color RGB before emissive.
- Multiply opacity by Niagara Particle Color alpha and always connect `MP_Opacity`.
- Bound animated-noise, Fresnel, and main-texture modulation to a non-zero `0.35..1.0`
  range so an accent/mask cannot erase all warm output.
- Strengthen master reuse verification: emissive + opacity connected, additive,
  unlit, and Particle Color reachable from both compiled output trees. Old black
  masters therefore fail reuse verification and are recreated.
- Extend core and POC ribbon tests to assert those compiled graph invariants.

## Exact orch commands

Run with the Unreal Editor closed and after replacing `<WS08_COMMIT_SHA>`:

```powershell
git -C "$UEREMCP_ROOT-ws01" cherry-pick <WS08_COMMIT_SHA>

& "$UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
  REEditor Win64 Development `
  "$UEREMCP_LEGACY_PROJECT\RE.uproject" `
  -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles -Module=UeremcpMaterial

Set-Location "$UEREMCP_ROOT-ws01"
pwsh tests/run_poc_b_fireball.ps1
pwsh tests/run_poc_b10_visible_render.ps1
```

The create command must recreate/revalidate the six `/Game/__UeremcpPoc/Materials/`
MIs and their stale masters before B10. Do not soften B10 warm thresholds. This handoff
does not claim overall POC-B completion.
