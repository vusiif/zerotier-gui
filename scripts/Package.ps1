param(
    [string]$Version = '0.1.0',
    [string]$Rar = 'C:\Program Files\WinRAR\Rar.exe',
    [string]$ISCC = 'C:\Program Files\Inno Setup 7\ISCC.exe'
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+([.-][A-Za-z0-9.-]+)?$') { throw 'Invalid package version.' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$payload = Join-Path $root 'dist'
$output = Join-Path $root 'packages'
foreach ($tool in @($Rar, $ISCC)) { if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Missing tool: $tool" } }
$manifest = Get-Content -LiteralPath (Join-Path $payload 'manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest) {
    $file = [IO.Path]::GetFullPath((Join-Path $payload $entry.File))
    if (-not $file.StartsWith($payload + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path escaped dist.' }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.SHA256) { throw "Payload changed: $($entry.File)" }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$base = "ZeroTier-GUI-$Version-Windows-x64"
$sfx = Join-Path $output "$base-SFX.exe"
$setup = Join-Path $output "$base-Setup.exe"
foreach ($file in @($sfx, $setup)) { if (Test-Path -LiteralPath $file) { throw "Output already exists: $file. Use another version or preserve the old package first." } }
$comment = Join-Path $output 'sfx-comment.txt'
# Persistent extraction directory; never TempMode (the GUI needs its DLLs).
@'
; WinRAR SFX configuration
Title=ZeroTier GUI - Extract package
Overwrite=0
Text
Extract all files to a writable directory. Run zerotier_gui.exe afterwards.
If Windows reports a missing Visual C++ runtime, run vc_redist.x64.exe first.
This package does not install or uninstall ZeroTier itself.
'@ | Set-Content -LiteralPath $comment -Encoding ascii
Push-Location $payload
try {
    & $Rar a -r -m5 "-sfx$(Join-Path (Split-Path $Rar -Parent) 'Default.SFX')" "-z$comment" $sfx '*'
    if ($LASTEXITCODE -ne 0) { throw "WinRAR failed: $LASTEXITCODE" }
} finally { Pop-Location }
& $Rar t $sfx
if ($LASTEXITCODE -ne 0) { throw 'SFX archive integrity check failed.' }
& $ISCC "/DPackageVersion=$Version" "/DPayloadDir=$payload" "/DPackageOutput=$output" (Join-Path $root 'packaging/zerotier-gui.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed: $LASTEXITCODE" }
Get-FileHash -LiteralPath $sfx, $setup -Algorithm SHA256 |
    Select-Object @{Name='File';Expression={Split-Path $_.Path -Leaf}}, Hash |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "$base-SHA256.json") -Encoding utf8
Write-Output "Packages ready: $output"
