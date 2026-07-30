# WS-11 orchestration for POC A (A1-A11) and POC B restart durability (B8).
# Domain-owned filters emit UEREMCP_POC_EVIDENCE=<compact JSON>.
param(
    [ValidateSet("A", "B8")]
    [string]$Scenario = "A",
    [string]$PocAFilter = "UEREMCP.Blueprint.POCA.CompleteRoundTrip",
    [string]$B8CreateFilter = "UEREMCP.Niagara.POCB.Restart.Create",
    [string]$B8VerifyFilter = "UEREMCP.Niagara.POCB.Restart.Verify",
    [string]$Project = "$UEREMCP_LEGACY_PROJECT\RE.uproject",
    [string]$EngineCmd = "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$EvidenceOutput = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$EditorRunner = Join-Path $PSScriptRoot "run_editor_tests.ps1"
$EvidenceParser = Join-Path $PSScriptRoot "poc_evidence.py"

function Invoke-EvidenceFilter {
    param(
        [string]$Filter,
        [string]$EvidenceScenario
    )

    $output = & $EditorRunner `
        -Project $Project `
        -EngineCmd $EngineCmd `
        -KeepUeremcp `
        -NoProbe `
        -Filter $Filter 2>&1
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
        return [ordered]@{
            filter = $Filter
            outcome = "failed"
            blocker = "editor runner produced no readable log"
            editor_exit = $editorExit
        }
    }

    $logText = Get-Content -Raw -Path $logPath
    if ($logText -notmatch "UEREMCP_POC_EVIDENCE=") {
        $filterFound = $logText -match "Found\s+[1-9][0-9]*\s+automation tests"
        return [ordered]@{
            filter = $Filter
            outcome = if ($filterFound) { "failed" } else { "skipped" }
            blocker = if ($filterFound) {
                "filter ran without a machine-readable evidence marker"
            }
            else {
                "domain handoff filter is absent from the active plugin binary"
            }
            editor_exit = $editorExit
            log = $logPath
        }
    }

    $validationJson = & python $EvidenceParser `
        --scenario $EvidenceScenario `
        --log $logPath
    $parserExit = $LASTEXITCODE
    $validation = $validationJson | ConvertFrom-Json
    if ($parserExit -ne 0 -or -not $validation.valid) {
        return [ordered]@{
            filter = $Filter
            outcome = "failed"
            blocker = "evidence failed strict validation"
            errors = @($validation.errors)
            editor_exit = $editorExit
            log = $logPath
        }
    }

    return [ordered]@{
        filter = $Filter
        outcome = $validation.evidence.outcome
        evidence = $validation.evidence
        editor_exit = $editorExit
        log = $logPath
    }
}

$results = @()
if ($Scenario -eq "A") {
    $results += Invoke-EvidenceFilter `
        -Filter $PocAFilter `
        -EvidenceScenario "poc_a"
}
else {
    # These are intentionally separate UnrealEditor-Cmd launches. The verify phase
    # cannot PASS merely by observing state in the process that created it.
    $create = Invoke-EvidenceFilter `
        -Filter $B8CreateFilter `
        -EvidenceScenario "poc_b8_create"
    $results += $create

    if ($create["outcome"] -eq "pass") {
        $verify = Invoke-EvidenceFilter `
            -Filter $B8VerifyFilter `
            -EvidenceScenario "poc_b8_verify"

        if ($verify["outcome"] -eq "pass") {
            $createId = $create["evidence"].checkpoint.id
            $verifyId = $verify["evidence"].checkpoint.id
            $createAssets = @($create["evidence"].checkpoint.assets | Sort-Object)
            $verifyAssets = @($verify["evidence"].checkpoint.assets | Sort-Object)
            $assetsMatch = (ConvertTo-Json -Compress $createAssets) -eq `
                (ConvertTo-Json -Compress $verifyAssets)
            if ($createId -ne $verifyId -or -not $assetsMatch) {
                $verify["outcome"] = "failed"
                $verify["blocker"] = "restart verify checkpoint does not match create phase"
            }
        }
        $results += $verify
    }
}

$outcomes = @($results | ForEach-Object { $_["outcome"] })
$summary = [ordered]@{
    scenario = $Scenario
    outcome = if ($outcomes -contains "failed" -or $outcomes -contains "fail") {
        "failed"
    }
    elseif ($outcomes -contains "skipped" -or $outcomes -contains "skip") {
        "skipped"
    }
    else {
        "pass"
    }
    results = $results
}

$summaryJson = $summary | ConvertTo-Json -Depth 12
$summaryJson
if ($EvidenceOutput) {
    $parent = Split-Path -Parent $EvidenceOutput
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -Path $EvidenceOutput -Value $summaryJson -Encoding utf8
}

if ($summary.outcome -eq "failed") { exit 1 }
if ($summary.outcome -eq "skipped") { exit 2 }
exit 0
