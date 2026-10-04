$ErrorActionPreference = 'Stop'
$ztProgram = Join-Path $env:ProgramData 'ZeroTier/One/zerotier-one_x64.exe'
$results = foreach ($ztCommand in @('info', 'listnetworks', 'listpeers', 'listmoons')) {
    $output = & $ztProgram -q -j $ztCommand 2>&1
    $exitCode = $LASTEXITCODE
    try {
        $raw = $output -join "`n"
        $data = $raw | ConvertFrom-Json -ErrorAction Stop
        $shape = if ($ztCommand -eq 'info') { $raw.TrimStart().StartsWith('{') } else { $raw.TrimStart().StartsWith('[') }
        $count = if ($raw -match '^\s*\[\s*\]\s*$') { 0 } else { @($data | Where-Object { $null -ne $_ }).Count }
        $version = $null; $online = $null
        if ($ztCommand -eq 'info') { $version = $data.version; $online = $data.online }
        [PSCustomObject]@{Command=$ztCommand; ExitCode=$exitCode; Json=$true; ExpectedShape=$shape; Count=$count; Version=$version; Online=$online}
    } catch {
        # Save diagnostics, never the authentication token or raw network/peer identities.
        [PSCustomObject]@{Command=$ztCommand; ExitCode=$exitCode; Json=$false; Error='CLI did not return valid JSON.'}
    }
}
New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot '../cmake-build-ela') -Force | Out-Null
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $PSScriptRoot '../cmake-build-ela/installed-verification.json') -Encoding utf8
if (@($results | Where-Object { $_.ExitCode -ne 0 -or -not $_.Json -or -not $_.ExpectedShape }).Count) {
    throw 'Read-only verification failed. See cmake-build-ela/installed-verification.json.'
}
