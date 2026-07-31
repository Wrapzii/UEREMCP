# Runs the WS-11 single-create fireball inline-material editor proof.
param(
    [string]$Scaffold = "",
    [string]$MaterialsFixture = "",
    [string]$Project = "$UEREMCP_PROJECT\visualtest.uproject",
    [string]$EngineCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$EditorRunner = Join-Path $PSScriptRoot "run_editor_tests.ps1"
if (-not $Scaffold) {
    $Scaffold = Join-Path $RepoRoot "schemas\domains\niagara\fixtures\poc_b_editor_gate_scaffold.json"
}
if (-not $MaterialsFixture) {
    $MaterialsFixture = Join-Path $RepoRoot "schemas\domains\niagara\fixtures\poc_b_fireball_materials.json"
}

$missing = @($Scaffold, $MaterialsFixture) | Where-Object { -not (Test-Path $_) }
if ($missing.Count -gt 0) {
    [ordered]@{
        gate = "POC B single-create inline materials"
        outcome = "skipped"
        blocker = "required fixture not merged into this worktree"
        missing = $missing
    } | ConvertTo-Json -Depth 4
    exit 2
}

$output = & $EditorRunner `
    -Project $Project `
    -EngineCmd $EngineCmd `
    -KeepUeremcp `
    -NoProbe `
    -Filter "UEREMCP.Niagara.POCB.FireballInlineMaterials" `
    -PocBScaffold $Scaffold `
    -PocBMaterials $MaterialsFixture 2>&1
$editorExit = $LASTEXITCODE
$output | ForEach-Object { Write-Host $_ }

$logLine = $output |
    Where-Object { "$_" -match "^UEREMCP_EDITOR_LOG=(.+)$" } |
    Select-Object -First 1
$logPath = if ($logLine -and "$logLine" -match "^UEREMCP_EDITOR_LOG=(.+)$") {
    $Matches[1].Trim()
}
else {
    ""
}
if (-not $logPath -or -not (Test-Path $logPath)) {
    [ordered]@{
        gate = "POC B single-create inline materials"
        outcome = "failed"
        blocker = "editor runner produced no readable log"
        editor_exit = $editorExit
    } | ConvertTo-Json -Depth 4
    exit 1
}

$log = Get-Content -Raw -Path $logPath
$marker = [regex]::Match(
    $log,
    "UEREMCP_POC_B_FIREBALL_OUTCOME=(PASS|FAIL|SKIP)(?:\s+([^\r\n]+))?")
if (-not $marker.Success) {
    $filterFound = $log -match "Found\s+[1-9][0-9]*\s+automation tests"
    [ordered]@{
        gate = "POC B single-create inline materials"
        outcome = if ($filterFound) { "failed" } else { "skipped" }
        blocker = if ($filterFound) {
            "filter ran without its evidence marker"
        }
        else {
            "validation filter is absent from the active orch plugin binary"
        }
        editor_exit = $editorExit
        log = $logPath
    } | ConvertTo-Json -Depth 4
    exit $(if ($filterFound) { 1 } else { 2 })
}

$outcome = $marker.Groups[1].Value.ToLowerInvariant()
[ordered]@{
    gate = "POC B single-create inline materials"
    outcome = $outcome
    detail = $marker.Groups[2].Value.Trim()
    editor_exit = $editorExit
    log = $logPath
    scope = "one editor tool invocation; not MCP transport or overall POC B proof"
} | ConvertTo-Json -Depth 4

if ($outcome -eq "fail") { exit 1 }
if ($outcome -eq "skip") { exit 2 }
exit 0
