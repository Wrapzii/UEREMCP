# Runs the ADR-0006 restart pair in two distinct editor processes.
param(
    [string]$Project = "$UEREMCP_LEGACY_PROJECT\RE.uproject",
    [string]$EngineCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$LogDir = ""
)

$ErrorActionPreference = "Stop"
$runner = Join-Path $PSScriptRoot "run_editor_tests.ps1"
$common = @{
    Project = $Project
    EngineCmd = $EngineCmd
    KeepUeremcp = $true
    NoProbe = $true
}
if ($LogDir) {
    $common["LogDir"] = $LogDir
}

& $runner @common -Filter "UEREMCP.Validation.Idempotency.Restart.Create"
if ($LASTEXITCODE -ne 0) {
    throw "Idempotency restart create process failed with exit code $LASTEXITCODE"
}

& $runner @common -Filter "UEREMCP.Validation.Idempotency.Restart.Verify"
if ($LASTEXITCODE -ne 0) {
    throw "Idempotency restart verify process failed with exit code $LASTEXITCODE"
}

Write-Host "UEREMCP_IDEMPOTENCY_RESTART_PAIR=pass"
