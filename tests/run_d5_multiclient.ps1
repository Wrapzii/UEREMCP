# Runs the genuine RE Pattern B listen-server + two-client proof.
#
# The automation invokes CastAbility on a non-authority client and requires:
# server stamina spend, owning-client stamina replication, a second client
# observing ability-specific Niagara, server projectile damage, and the second
# client observing replicated target health. No screenshot is used as proof.

param(
    [string]$EditorCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$ArtifactDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ArtifactDir) {
    $ArtifactDir = Join-Path $RepoRoot "tests\integration\_artifacts"
}
New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null

$activeEditor = Get-Process -Name "UnrealEditor", "UnrealEditor-Cmd" -ErrorAction SilentlyContinue
if ($activeEditor) {
    throw "Close running Unreal processes before D5 automation (PID $($activeEditor.Id -join ','))."
}

$artifact = Join-Path $ArtifactDir "d5_pattern_b_multiclient.json"
if (Test-Path $artifact) {
    Remove-Item -Force $artifact
}

# Rendered PIE keeps Niagara cosmetics spawnable; NullRHI often yields zero
# UNiagaraComponents even when Multicast_AbilityCosmetics runs. Montage proof
# remains available either way.
$runner = Join-Path $PSScriptRoot "run_editor_tests.ps1"
$output = & $runner `
    -EngineCmd $EditorCmd `
    -Filter "UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE" `
    -KeepUeremcp `
    -NoProbe `
    -WithRendering `
    -ExtraArgs @("-UeremcpD5Output=`"$artifact`"") 2>&1
$output | ForEach-Object { Write-Output $_ }

$outcome = $null
$marker = $output |
    Select-String -Pattern "UEREMCP_D5_OUTCOME=(PASS|FAIL)" |
    Select-Object -Last 1
if (-not $marker) {
    $logLine = $output |
        Select-String -Pattern "UEREMCP_EDITOR_LOG=(.+)$" |
        Select-Object -Last 1
    if ($logLine) {
        $logPath = $logLine.Matches[0].Groups[1].Value.Trim()
        if (Test-Path $logPath) {
            $marker = Select-String -Path $logPath -Pattern "UEREMCP_D5_OUTCOME=(PASS|FAIL)" |
                Select-Object -Last 1
        }
    }
}
if (-not $marker -and (Test-Path $artifact)) {
    $evidenceProbe = Get-Content -Raw $artifact | ConvertFrom-Json
    if ($evidenceProbe.status -eq "pass") {
        $outcome = "PASS"
    }
    elseif ($evidenceProbe.status) {
        $outcome = "FAIL"
    }
}
if (-not $marker -and -not $outcome) {
    Write-Output "UEREMCP_D5_RUNNER=FAIL reason=outcome_marker_missing"
    exit 1
}
if ($marker) {
    $outcome = $marker.Matches[0].Groups[1].Value
}
Write-Output "UEREMCP_D5_RUNNER=$outcome artifact=$artifact"
if ($outcome -ne "PASS" -or -not (Test-Path $artifact)) {
    exit 1
}

$evidence = Get-Content -Raw $artifact | ConvertFrom-Json
if ($evidence.status -ne "pass" `
    -or $evidence.remote_clients -ne 2 `
    -or -not $evidence.server_authority_accepted `
    -or -not $evidence.second_client_observed_cast_effect `
    -or -not $evidence.server_applied_damage `
    -or -not $evidence.second_client_observed_replicated_damage) {
    Write-Output "UEREMCP_D5_RUNNER=FAIL reason=evidence_contract_failed"
    exit 1
}
