# Isolated, timeout-bounded diagnostic; never loads external code inside FL Studio.
# Usage: .\diagnose-external-plugin.ps1 -Plugin 'C:\Program Files\Common Files\VST3\Example.vst3'
param(
    [Parameter(Mandatory=$true)][string]$Plugin,
    [string]$Probe=(Join-Path $PSScriptRoot 'SkellPluginDiagnostic.exe'),
    [ValidateRange(5,120)][int]$TimeoutSeconds=25
)
$ErrorActionPreference='Stop'
if (!(Test-Path -LiteralPath $Probe -PathType Leaf)) {throw "Diagnostic executable not found: $Probe"}
if (!(Test-Path -LiteralPath $Plugin)) {throw "Plugin not found: $Plugin"}
$log=Join-Path $env:TEMP ('skell-vst3-'+[guid]::NewGuid().ToString('N'))
$out="$log.out.txt";$err="$log.err.txt"
# Start-Process keeps the scanned plugin outside the DAW process. Kill the
# probe if a plugin deadlocks; do not automatically restart the DAW.
$p=Start-Process -FilePath $Probe -ArgumentList @('--vst3', ('"'+$Plugin+'"')) -NoNewWindow -RedirectStandardOutput $out -RedirectStandardError $err -PassThru
try {
    if (!$p.WaitForExit($TimeoutSeconds*1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        Write-Host "FAIL timeout after $TimeoutSeconds seconds: $Plugin"
        exit 124
    }
    if (Test-Path $out) {Get-Content -LiteralPath $out}
    if (Test-Path $err) {Get-Content -LiteralPath $err}
    Write-Host "Exit code: $($p.ExitCode)"
    exit $p.ExitCode
} finally {
    Remove-Item -LiteralPath $out,$err -Force -ErrorAction SilentlyContinue
}
