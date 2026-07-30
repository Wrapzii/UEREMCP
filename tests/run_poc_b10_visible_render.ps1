# Runs the POC B B10 viewport proof with rendering enabled.
#
# The automation filter performs a before/after viewport pixel comparison and
# writes a supplementary PNG. A screenshot without the programmatic pixel gate
# is not a PASS.

param(
    [string]$Editor = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor.exe",
    [string]$ArtifactDir = "",
    [string]$SystemPath = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ArtifactDir) {
    $ArtifactDir = Join-Path $RepoRoot "tests\integration\_artifacts"
}
New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null

$activeEditor = Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue
if ($activeEditor) {
    throw "Close the running Unreal Editor before the B10 automation run (PID $($activeEditor.Id -join ','))."
}

$artifactName = if ($SystemPath) { "poc_b10_canary.png" } else { "poc_b10_fireball.png" }
$artifact = Join-Path $ArtifactDir $artifactName
$extraArgs = @(
    "-DisablePlugins=VoxelFree",
    "-UeremcpPocB10Output=`"$artifact`""
)
if ($SystemPath) {
    $extraArgs += "-UeremcpPocB10System=`"$SystemPath`""
}
$runner = Join-Path $PSScriptRoot "run_editor_tests.ps1"
$output = & $runner `
    -EngineCmd $Editor `
    -Filter "UEREMCP.Niagara.POCB.VisibleRender" `
    -KeepUeremcp `
    -NoProbe `
    -WithRendering `
    -ExtraArgs $extraArgs 2>&1
$output | ForEach-Object { Write-Output $_ }

$marker = $output |
    Select-String -Pattern "UEREMCP_POC_B10_OUTCOME=(PASS|FAIL|SKIP)" |
    Select-Object -Last 1
if (-not $marker) {
    Write-Output "UEREMCP_POC_B10_RUNNER=FAIL reason=outcome_marker_missing"
    exit 1
}

$outcome = $marker.Matches[0].Groups[1].Value
Write-Output "UEREMCP_POC_B10_RUNNER=$outcome artifact=$artifact"
if ($outcome -ne "PASS" -or -not (Test-Path $artifact)) {
    exit 1
}
