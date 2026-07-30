# UEREMCP editor integration test runner (WS-11 / RB-14).
#
# Launches UnrealEditor-Cmd against the RE project, runs a named automation
# filter, then quits. Does NOT destroy user content: tests must use
# /Game/__UeremcpTests/ only.
#
# Prerequisites:
#   - Preferred: UeremcpValidation registered in UEREMCP.uplugin (see
#     docs/proposals/ws-11-register-validation-module.md) and compiled into RE.
#   - Interim: standalone probe plugin junctioned at
#     $PROJ/Plugins/UeremcpValidationProbe -> tests/integration/editor_plugin/...
#     (EnabledByDefault: false; this script enables it via -EnablePlugins).
#
# Known blocker (2026-07-29): if UEREMCP is Enabled in RE.uproject but
# UeremcpCore is not built, the editor aborts before any automation runs.
# Default -DisablePlugins UEREMCP avoids that until WS-03 ships a loadable plugin.
# Pass -KeepUeremcp to leave it enabled.
#
# Usage:
#   pwsh tests/run_editor_tests.ps1
#   pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.Validation.Harness.Smoke"
#   pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.Validation.Rollback.MultiAssetDiscard"
#   pwsh tests/run_editor_tests.ps1 -Filter "AI.ToolsetRegistry.Sandbox.Library"
#
param(
    [string]$Project = "$UEREMCP_LEGACY_PROJECT\RE.uproject",
    [string]$EngineCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$Filter = "UEREMCP.Validation",
    [string]$LogDir = "",
    [string]$EnablePlugins = "UeremcpValidationProbe",
    [string]$DisablePlugins = "UEREMCP",
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

$proc = Start-Process -FilePath $EngineCmd -ArgumentList $argList -Wait -PassThru
Write-Host "Exit code: $($proc.ExitCode)"

# Surface automation summary lines for agents that cannot open the full log.
if (Test-Path $LogFile) {
    Write-Host "---- automation result lines ----"
    Select-String -Path $LogFile -Pattern "Test Completed|FAIL|SUCCESS|Error:|Q1 |Q3 |Rollback\.MultiAssetDiscard|UEREMCP\.Validation|Sandbox\.Library|Automation Test Queue" |
        Select-Object -Last 80 |
        ForEach-Object { $_.Line }
}

exit $proc.ExitCode
