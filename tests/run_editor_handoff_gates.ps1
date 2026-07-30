# Runs WS-11 editor gates handed off by WS-06 and WS-07.
# Missing merged source/binaries are reported as SKIP, never PASS.
param(
    [ValidateSet("All", "Blueprint", "Niagara")]
    [string]$Gate = "All",
    [string]$Scaffold = "",
    [string]$Project = "$UEREMCP_LEGACY_PROJECT\RE.uproject",
    [string]$EngineCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$EditorRunner = Join-Path $PSScriptRoot "run_editor_tests.ps1"
if (-not $Scaffold) {
    $Scaffold = Join-Path $RepoRoot "schemas\domains\niagara\fixtures\poc_b_editor_gate_scaffold.json"
}

function Invoke-HandoffGate {
    param(
        [string]$Name,
        [string]$Filter,
        [string]$Marker,
        [string]$PocBScaffold = "",
        [string[]]$ExtraArgs = @()
    )

    $runnerArgs = @(
        "-NoProfile", "-File", $EditorRunner,
        "-Project", $Project,
        "-EngineCmd", $EngineCmd,
        "-KeepUeremcp", "-NoProbe",
        "-Filter", $Filter
    )
    if ($ExtraArgs.Count -gt 0) {
        $runnerArgs += "-ExtraArgs"
        $runnerArgs += $ExtraArgs
    }
    if ($PocBScaffold) {
        $runnerArgs += "-PocBScaffold"
        $runnerArgs += $PocBScaffold
    }

    $output = & pwsh @runnerArgs 2>&1
    $editorExit = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }

    $logLine = $output | Where-Object { "$_" -match "^Log:\s+(.+)$" } | Select-Object -First 1
    $logPath = if ($logLine -and "$logLine" -match "^Log:\s+(.+)$") { $Matches[1].Trim() } else { "" }
    if (-not $logPath -or -not (Test-Path $logPath)) {
        return [ordered]@{
            gate = $Name; filter = $Filter; outcome = "failed"
            blocker = "editor runner produced no readable log"; editor_exit = $editorExit
        }
    }

    $log = Get-Content -Raw -Path $logPath
    $markerMatch = [regex]::Match($log, "$([regex]::Escape($Marker))=(PASS|FAIL|SKIP)(?:\s+([^\r\n]+))?")
    $completion = [regex]::Match(
        $log,
        "Test Completed\. Result=\{([^}]+)\}[^\r\n]*Path=\{$([regex]::Escape($Filter))\}")

    if ($markerMatch.Success) {
        $outcome = $markerMatch.Groups[1].Value.ToLowerInvariant()
        return [ordered]@{
            gate = $Name; filter = $Filter; outcome = $outcome
            detail = $markerMatch.Groups[2].Value.Trim()
            automation_result = if ($completion.Success) { $completion.Groups[1].Value } else { $null }
            editor_exit = $editorExit; log = $logPath
        }
    }

    if ($completion.Success -and $completion.Groups[1].Value -ne "Success") {
        return [ordered]@{
            gate = $Name; filter = $Filter; outcome = "failed"
            blocker = "automation test completed with $($completion.Groups[1].Value)"
            editor_exit = $editorExit; log = $logPath
        }
    }

    return [ordered]@{
        gate = $Name; filter = $Filter; outcome = "skipped"
        blocker = "filter/marker absent from active RE plugin binary; merge and rebuild handoff without retargeting the RE junction"
        editor_exit = $editorExit; log = $logPath
    }
}

$results = @()
if ($Gate -in @("All", "Blueprint")) {
    $results += Invoke-HandoffGate `
        -Name "WS-06 MutatingDispatch enable" `
        -Filter "UEREMCP.Validation.Blueprint.MutatingDispatchGate" `
        -Marker "UEREMCP_BLUEPRINT_DISPATCH_OUTCOME"
}
if ($Gate -in @("All", "Niagara")) {
    if (-not (Test-Path $Scaffold)) {
        $results += [ordered]@{
            gate = "WS-07 POC B B7 scaffold"; outcome = "skipped"
            blocker = "scaffold not found: $Scaffold"
        }
    }
    else {
        $scaffoldJson = Get-Content -Raw -Path $Scaffold | ConvertFrom-Json
        if ($scaffoldJson.probe_paths.system -ne "/Game/__UeremcpTests/NS_POCB_FireballProbe") {
            throw "Refusing non-canonical POC B scaffold target: $($scaffoldJson.probe_paths.system)"
        }
        $results += Invoke-HandoffGate `
            -Name "WS-07 POC B B7 scaffold" `
            -Filter "UEREMCP.Niagara.POCB.SixEmitterGateScaffold" `
            -Marker "UEREMCP_POC_B_GATE_OUTCOME" `
            -PocBScaffold $Scaffold
    }
}

$results | ConvertTo-Json -Depth 5
$outcomes = @($results | ForEach-Object { $_["outcome"] })
if ($outcomes -contains "failed" -or $outcomes -contains "fail") { exit 1 }
if ($outcomes -contains "skipped" -or $outcomes -contains "skip") { exit 2 }
exit 0
