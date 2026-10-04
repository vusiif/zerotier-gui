$ErrorActionPreference = 'Stop'
$ztProgram = Join-Path $env:ProgramData 'ZeroTier/One/zerotier-one_x64.exe'
$results = foreach ($ztCommand in @('info', 'listnetworks', 'listpeers', 'listmoons')) {
    $output = & $ztProgram -q -j $ztCommand 2>&1
    $exitCode = $LASTEXITCODE
    try {
        $data = ($output -join "`n") | ConvertFrom-Json -ErrorAction Stop
        [PSCustomObject]@{Command=$ztCommand; ExitCode=$exitCode; Json=$true; Count=@($data | Where-Object { $null -ne $_ }).Count; Version=$(if ($ztCommand -eq 'info') { $data.version }); Online=$(if ($ztCommand -eq 'info') { $data.online })}
    } catch {
        # Save diagnostics, never the authentication token or raw network/peer identities.
        [PSCustomObject]@{Command=$ztCommand; ExitCode=$exitCode; Json=$false; Error=$_.Exception.Message}
    }
}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $PSScriptRoot '../cmake-build-ela/installed-verification.json') -Encoding utf8
