# Read-only installed VST3 identity check. Never changes plugin files or DAW state.
# Usage: .\verify-installed-skell-preset.ps1 -BuildInfo '.\Skell-Preset-BUILD-INFO.txt'
param(
    [string]$Root = '',
    [string]$BuildInfo = '',
    [string]$ExpectedSha256 = ''
)
$ErrorActionPreference = 'Stop'
$expected = $ExpectedSha256.Trim().ToUpperInvariant()
if ($BuildInfo) {
    if (!(Test-Path -LiteralPath $BuildInfo -PathType Leaf)) {throw "Build info not found: $BuildInfo"}
    $contents = Get-Content -LiteralPath $BuildInfo -Raw
    if ($contents -match '(?im)^Binary SHA256:\s*([0-9a-f]{64})\s*$') {
        if (!$expected) {$expected = $Matches[1].ToUpperInvariant()}
    } else {throw 'Build info missing Binary SHA256'}
    if ($contents -match '(?im)^PR Source Commit:\s*([0-9a-f]{40})\s*$') {
        Write-Host "Expected source commit: $($Matches[1])"
    }
}
if ($expected -and $expected -notmatch '^[0-9A-F]{64}$') {
    throw 'ExpectedSha256 must be a 64-character hexadecimal SHA256 hash'
}
$searchRoots = @()
if ($Root) {
    $searchRoots += $Root
} else {
    if ($env:CommonProgramFiles) {$searchRoots += (Join-Path $env:CommonProgramFiles 'VST3')}
    if ($env:APPDATA) {$searchRoots += (Join-Path $env:APPDATA 'VST3')}
    if ($env:LOCALAPPDATA) {
        $searchRoots += (Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3')
    }
}
$found = @()
foreach ($base in ($searchRoots | Select-Object -Unique)) {
    if (!(Test-Path -LiteralPath $base -PathType Container)) {continue}
    $bundles = @()
    if ((Split-Path -Leaf $base) -ieq 'Skell-Preset.vst3') {
        $bundles += Get-Item -LiteralPath $base
    } else {
        $bundles += @(Get-ChildItem -LiteralPath $base -Directory -Filter 'Skell-Preset.vst3' -Recurse -Depth 3 -ErrorAction SilentlyContinue)
    }
    foreach ($bundle in $bundles) {
        $bin = Join-Path $bundle.FullName 'Contents\x86_64-win\Skell-Preset.vst3'
        if (!(Test-Path -LiteralPath $bin -PathType Leaf)) {
            Write-Warning "Incomplete Skell-Preset bundle: $($bundle.FullName)"
            continue
        }
        $hash = (Get-FileHash -LiteralPath $bin -Algorithm SHA256).Hash.ToUpperInvariant()
        $same = !$expected -or $hash -eq $expected
        $found += [PSCustomObject]@{
            Bundle = $bundle.FullName
            SHA256 = $hash
            Match = $same
        }
    }
}
if ($found.Count -eq 0) {
    Write-Host 'NO INSTALLED SKELL PRESET BUNDLE FOUND. Check your VST3 installation path.'
    exit 2
}
$found | Format-List | Out-Host
if (!$expected) {
    Write-Warning 'No expected build SHA256 supplied; this inventory cannot identify the source commit.'
    exit 0
}
if (@($found | Where-Object {$_.Match}).Count -eq 0) {
    Write-Host 'MISMATCH: no installed copy matches the supplied exact-build SHA256.'
    exit 3
}
Write-Host 'PASS: at least one installed VST3 matches the exact-build binary SHA256.'
exit 0
