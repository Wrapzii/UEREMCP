# WS-01: RE UEREMCPTransportTest vs shipping UeremcpTransport (CS0101)

**Date:** 2026-07-30  
**Status:** Resolved on RE project (local); documented here.

## Symptom

REEditor Win64 Development failed with:

`	ext
error CS0101: The namespace '<global namespace>' already contains a definition for 'UeremcpTransport'
`

in Plugins\UEREMCP\Source\UeremcpTransport\UeremcpTransport.Build.cs, because Unreal Build Tool
loaded **two** ModuleRules classes named UeremcpTransport.

## Root cause

$UEREMCP_LEGACY_PROJECT\Plugins\UEREMCPTransportTest\ is a **WS-04 sidecar**
probe plugin (see its FriendlyName / Description: transport automation, not shipping). It duplicated
the shipping module layout:

| Artifact | Shipping (UEREMCP) | Sidecar (UEREMCPTransportTest) |
|---|---|---|
| Plugin folder | RE\Plugins\UEREMCP (junction → UEREMCP-ws01\Plugins\UEREMCP) | RE\Plugins\UEREMCPTransportTest |
| Module directory | Source\UeremcpTransport\ | Source\UeremcpTransport\ (same name) |
| Build.cs class | public class UeremcpTransport | public class UeremcpTransport (collision) |
| uplugin module Name | UeremcpTransport (in UEREMCP.uplugin) | UeremcpTransport (in sidecar uplugin) |

RE.uproject did **not** list UEREMCPTransportTest; the sidecar had EnabledByDefault: false.
UBT still discovers any *.uplugin under Project/Plugins/** and compiles module rules, so the clash
persisted.

The sidecar content matches the historical WS-04 transport probe (JobConstraints, automation tests,
	ransport_job_handoff.json) that now lives in the shipping plugin after WS-04 merge (72db366).
The RE-local copy is **obsolete**.

## Action taken (RE project — not in this git repo)

On $UEREMCP_LEGACY_PROJECT\Plugins\:

1. Renamed folder UEREMCPTransportTest → _disabled_UEREMCPTransportTest.
2. Renamed UEREMCPTransportTest.uplugin → UEREMCPTransportTest.uplugin.disabled so UBT no longer
   discovers the plugin.

**Not changed:** RE\Plugins\UEREMCP junction still targets
$UEREMCP_ROOT-ws01\Plugins\UEREMCP.

## Build verification

Command (editor closed):

`at
"$UE_ROOT\Engine\Build\BatchFiles\Build.bat" REEditor Win64 Development "-Project=$UEREMCP_LEGACY_PROJECT\RE.uproject" -NoHotReloadFromIDE -WaitMutex
`

- **CS0101 / duplicate UeremcpTransport:** absent after disable (transport clash **unblocked**).
- **Overall:** Failed (OtherCompilationError) — remaining errors in
  Plugins\UEREMCP\Source\UeremcpTemplates\ (UeremcpTemplateService.h/.cpp, FJsonObject / TSharedPtr
  template issues). Separate from this clash; WS-15 / templates workstream.

## Alternatives (if sidecar needed again)

Rename sidecar module **and** folder to a unique name (e.g. UeremcpTransportTestModule) and update
the sidecar uplugin Modules[].Name. Do **not** reuse UeremcpTransport while shipping transport
is registered in UEREMCP.uplugin.

## Ownership

- Shipping transport: WS-04 sources, WS-03 uplugin registration (closed).
- This note: WS-01 orchestration / RE integration hygiene.
