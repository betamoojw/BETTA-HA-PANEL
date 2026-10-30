# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
# Native Windows counterpart to build.sh; activate ESP-IDF before invoking.
param(
    [ValidateSet('panel4', 'panel10', 'paneljc', 'paneljc-v2', 'tab5', 'panels3')]
    [string]$Variant
)
$ErrorActionPreference = 'Stop'
# Keep this a simple script: advanced parameters reserve IDF's -p for PowerShell.
$IdfArguments = $args
if (-not $Variant) { throw 'Usage: ./scripts/build.ps1 <variant> [idf.py arguments]' }
$targetChip = 'esp32p4'
$defaults = "config/sdkconfig.defaults;config/sdkconfig.defaults.$Variant"
if ($Variant -eq 'panels3') {
    $targetChip = 'esp32s3'
    $defaults = 'config/sdkconfig.defaults.panels3'
} elseif ($Variant -eq 'paneljc-v2') {
    $defaults = 'config/sdkconfig.defaults;config/sdkconfig.defaults.paneljc;config/sdkconfig.defaults.paneljc.v2'
}
if ($env:EXTRA_DEFAULTS) { $defaults += ';' + $env:EXTRA_DEFAULTS }
if (-not $env:IDF_PATH -or -not $env:IDF_PYTHON_ENV_PATH) {
    throw 'Activate the ESP-IDF PowerShell environment first.'
}
$repoRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repoRoot
try {
    & "$env:IDF_PYTHON_ENV_PATH/Scripts/python.exe" "$env:IDF_PATH/tools/idf.py" `
        --preset $Variant -B "build/$Variant" -D "IDF_TARGET=$targetChip" `
        -D "SDKCONFIG_DEFAULTS=$defaults" -D "SDKCONFIG=$repoRoot/build/$Variant/sdkconfig" @IdfArguments
    $buildExit = $LASTEXITCODE
    if ($buildExit -eq 0) {
        $archive = Join-Path $repoRoot ".local/elf/$Variant"
        foreach ($elf in Get-ChildItem "build/$Variant/betta-ha-panel*.elf" -ErrorAction SilentlyContinue) {
            New-Item -ItemType Directory -Force $archive | Out-Null
            $digest = (Get-FileHash -LiteralPath $elf.FullName -Algorithm SHA256).Hash.Substring(0, 9).ToLowerInvariant()
            $destination = Join-Path $archive "$digest.elf"
            if (-not (Test-Path -LiteralPath $destination)) {
                Copy-Item -LiteralPath $elf.FullName -Destination $destination
            }
        }
    }
} finally { Pop-Location }
exit $buildExit
