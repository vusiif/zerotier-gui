param([string]$ReleaseDirectory = 'dist')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$releasePath = [IO.Path]::GetFullPath((Join-Path $projectRoot $ReleaseDirectory))
if (-not $releasePath.StartsWith($projectRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Release directory must stay inside the project.' }
$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run this smoke check as Administrator.' }
$before = (Get-Service -Name ZeroTierOneService).Status.ToString()
if ($before -ne 'Running') { throw 'Service must already be Running. This check must not start or stop it.' }
$exe = Join-Path $releasePath 'zerotier_gui.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Release executable is missing.' }
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class ReleaseWindowProbe {
    private delegate bool EnumProc(IntPtr window, IntPtr param);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr window, StringBuilder text, int max);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    public static IntPtr Find(uint process) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, param) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner != process) return true;
            var title = new StringBuilder(256); GetWindowText(window, title, title.Capacity);
            if (title.ToString() != "ZeroTier GUI") return true;
            found = window; return false;
        }, IntPtr.Zero);
        return found;
    }
}
'@
$savedPath = $env:PATH
$savedQtVariables = @{}
foreach ($name in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QT_QPA_PLATFORM')) {
    $savedQtVariables[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
$appProcess = $null; $window = [IntPtr]::Zero; $closed = $false; $code = $null
try {
    # Exclude SDK directories from this helper's environment, never change the user's PATH.
    $env:PATH = (($savedPath -split ';') | Where-Object { $_ -notmatch '(^|[/\\])Qt([/\\]|$)' }) -join ';'
    $appProcess = Start-Process -FilePath $exe -WorkingDirectory $releasePath -WindowStyle Hidden -PassThru
    for ($attempt = 0; $attempt -lt 40; ++$attempt) {
        $appProcess.Refresh(); if ($appProcess.HasExited) { break }
        $window = [ReleaseWindowProbe]::Find([uint32]$appProcess.Id)
        if ($window -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 250
    }
    if ($window -ne [IntPtr]::Zero) {
        Start-Sleep -Seconds 2
        [void][ReleaseWindowProbe]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        $closed = $appProcess.WaitForExit(6000)
    }
    if ($appProcess.HasExited) { $code = $appProcess.ExitCode }
} finally {
    if ($appProcess -and -not $appProcess.HasExited) { $appProcess.Kill(); $appProcess.WaitForExit() }
    $env:PATH = $savedPath
    foreach ($name in $savedQtVariables.Keys) { [Environment]::SetEnvironmentVariable($name, $savedQtVariables[$name], 'Process') }
}
$after = (Get-Service -Name ZeroTierOneService).Status.ToString()
$report = [PSCustomObject]@{MainWindowCreated=($window -ne [IntPtr]::Zero); GracefulClose=$closed; ExitCode=$code; ServiceBefore=$before; ServiceAfter=$after; QtSdkPathExcluded=$true}
$outputDirectory = Join-Path $projectRoot '.docs-tmp'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputDirectory 'release-verification.json') -Encoding utf8
$report | ConvertTo-Json
if (-not $report.MainWindowCreated -or -not $closed -or $code -ne 0 -or $after -ne 'Running') { throw 'Release smoke verification failed. See .docs-tmp/release-verification.json.' }
