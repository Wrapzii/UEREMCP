# UEREMCP editor integration test runner (WS-11 / RB-14).
#
# Launches UnrealEditor-Cmd against the RE project, runs a named automation
# filter, then quits. Does NOT destroy user content: tests must use
# /Game/__UeremcpTests/ only.
#
# Prerequisites (shipping path — C-3):
#   - UeremcpValidation registered in UEREMCP.uplugin (WS-03 proposal) AND
#     UeremcpCore loadable, then: -KeepUeremcp -NoProbe
#     Filter: UEREMCP.Validation.Rollback.MultiAssetDiscard
#     Source of truth: Plugins/UEREMCP/Source/UeremcpValidation/...
#
# Interim probe (engine-only / launch):
#   - UeremcpValidationProbe is launch-smoke only (no Rollback body).
#   - Default -DisablePlugins UEREMCP when Core is missing.
#   - Probe green ≠ shipping UEREMCP plugin gate (WS-14 C-3 / WS-01).
#
# Usage:
#   pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"
#   pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"
#   pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Niagara.Inspect"
#   pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UeremcpMaterial.Toolset"
#   pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.ValidationProbe.Launch.Smoke"
#   pwsh tests/run_editor_tests.ps1 -Filter "AI.ToolsetRegistry.Sandbox.Library" -NoProbe
#
param(
    [string]$Project = "$UEREMCP_LEGACY_PROJECT\RE.uproject",
    [string]$EngineCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$Filter = "UEREMCP.Validation",
    [string]$LogDir = "",
    [string]$EnablePlugins = "UeremcpValidationProbe",
    [string]$DisablePlugins = "UEREMCP",
    [string]$PocBScaffold = "",
    [string]$PocBMaterials = "",
    [string[]]$ExtraArgs = @(),
    [switch]$KeepUeremcp,
    [switch]$NoProbe
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Project)) { throw "Project not found: $Project" }
if (-not (Test-Path $EngineCmd)) { throw "UnrealEditor-Cmd not found: $EngineCmd" }

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $LogDir) {
    $LogDir = Join-Path $RepoRoot "tests\integration\_logs"
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "editor_$($Filter.Replace('.','_'))_$stamp.log"

$exec = "Automation RunTests $Filter; Quit"
Write-Host "Running: $Filter"
Write-Host "Log: $LogFile"

$argList = [System.Collections.Generic.List[string]]::new()
$argList.Add("`"$Project`"")
$argList.Add("-unattended")
$argList.Add("-nop4")
$argList.Add("-nosplash")
$argList.Add("-NullRHI")
$argList.Add("-nosound")
$argList.Add("-log")
$argList.Add("-AbsLog=`"$LogFile`"")
$argList.Add("-ExecCmds=`"$exec`"")

$effectiveEnable = if ($NoProbe) { "" } else { $EnablePlugins }
if ($effectiveEnable) {
    $argList.Add("-EnablePlugins=`"$effectiveEnable`"")
    Write-Host "EnablePlugins: $effectiveEnable"
}

$effectiveDisable = $DisablePlugins
if ($KeepUeremcp) {
    $effectiveDisable = ""
}
if ($effectiveDisable) {
    $argList.Add("-DisablePlugins=`"$effectiveDisable`"")
    Write-Host "DisablePlugins: $effectiveDisable"
}
if ($PocBScaffold) {
    $argList.Add("-UeremcpPocBScaffold=`"$PocBScaffold`"")
}
if ($PocBMaterials) {
    $argList.Add("-UeremcpPocBMaterials=`"$PocBMaterials`"")
}
foreach ($extraArg in $ExtraArgs) {
    if ($extraArg) {
        $argList.Add($extraArg)
    }
}

$proc = Start-Process -FilePath $EngineCmd -ArgumentList $argList -Wait -PassThru
Write-Host "Exit code: $($proc.ExitCode)"

# Surface automation summary lines for agents that cannot open the full log.
if (Test-Path $LogFile) {
    Write-Host "---- automation result lines ----"
    Select-String -Path $LogFile -Pattern "Test Completed|FAIL|SUCCESS|SKIP:|Error:|Q1 |Q3 |Rollback\.MultiAssetDiscard|UEREMCP\.Validation|UEREMCP\.Transport|UEREMCP\.Niagara\.Inspect|UeremcpMaterial\.Toolset|created_and_validated|Sandbox\.Library|Automation Test Queue|UEREMCP_POC_B_GATE_OUTCOME|UEREMCP_POC_B_FIREBALL_OUTCOME" |
        Select-Object -Last 80 |
        ForEach-Object { $_.Line }
}

exit $proc.ExitCode
