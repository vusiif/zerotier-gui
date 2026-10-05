param(
    [string]$BuildDirectory = 'cmake-build-ui-restore',
    [string]$QtDirectory = 'C:/Qt/6.11.2/msvc2022_64',
    [string]$RuntimeInstaller = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$projectPrefix = $projectRoot.TrimEnd('\') + '\'
if (-not $buildPath.StartsWith($projectPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Build directory must be inside the project.' }
$cache = Get-Content -LiteralPath (Join-Path $buildPath 'CMakeCache.txt') -Raw
if ($cache -notmatch 'CMAKE_BUILD_TYPE:STRING=Release') { throw 'Package a Release build only.' }
$qtConfigPath = (Join-Path $QtDirectory 'lib/cmake/Qt6').Replace('\', '/').TrimEnd('/')
if ($cache -notmatch ('(?m)^Qt6_DIR:PATH=' + [regex]::Escape($qtConfigPath) + '\r?$')) { throw 'Qt deploy kit must match the build kit.' }
$exe = Join-Path $buildPath 'zerotier_gui.exe'
$deployTool = Join-Path $QtDirectory 'bin/windeployqt.exe'
foreach ($file in @($exe, $deployTool)) { if (-not (Test-Path -LiteralPath $file)) { throw "Missing $file" } }
if (-not $RuntimeInstaller) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -products '*' -property installationPath
        if ($installation) {
            $RuntimeInstaller = Get-ChildItem -LiteralPath (Join-Path $installation 'VC/Redist/MSVC') -Recurse -Filter vc_redist.x64.exe |
                Sort-Object FullName | Select-Object -First 1 -ExpandProperty FullName
        }
    }
}
if (-not $RuntimeInstaller -or -not (Test-Path -LiteralPath $RuntimeInstaller -PathType Leaf)) {
    throw 'Specify -RuntimeInstaller with the Microsoft x64 Visual C++ redistributable.'
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$staging = Join-Path $projectRoot "dist-staging-$stamp"
$destination = Join-Path $projectRoot 'dist'
$backup = Join-Path $projectRoot "dist-backup-$stamp"
foreach ($path in @($staging, $destination, $backup)) {
    if (-not ([IO.Path]::GetFullPath($path)).StartsWith($projectPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Publish path escaped workspace.' }
}
New-Item -ItemType Directory -Path $staging | Out-Null
Copy-Item -LiteralPath $exe -Destination $staging
Copy-Item -LiteralPath $RuntimeInstaller -Destination (Join-Path $staging 'vc_redist.x64.exe')
& $deployTool --release --no-translations (Join-Path $staging 'zerotier_gui.exe')
if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed. Staging retained at $staging" }
$licenseDirectory = Join-Path $staging 'licenses'
New-Item -ItemType Directory -Path $licenseDirectory | Out-Null
$qtVersion = Split-Path (Split-Path $QtDirectory -Parent) -Leaf
$qtRoot = Split-Path (Split-Path $QtDirectory -Parent) -Parent
$qtDocs = Join-Path $qtRoot "Docs/Qt-$qtVersion/qtdoc"
foreach ($name in @('lgpl.html', 'gpl.html', 'licenses-used-in-qt.html')) {
    $file = Join-Path $qtDocs $name
    if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $licenseDirectory }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination (Join-Path $staging 'PROJECT-README.md')
Copy-Item -LiteralPath (Join-Path $projectRoot 'RESTORATION.md') -Destination (Join-Path $staging 'DEVELOPMENT-NOTES.md')
@"
ZeroTier GUI (Windows x64)

解压整个目录后运行 zerotier_gui.exe，不要单独移动 exe。
启动时自动检测 ZeroTier：未安装只显示独立安装窗口，点击 winget 安装按钮；检测到安装成功后自动进入管理界面。已安装则直接进入管理界面。
winget 不可用时，请更新 Microsoft Store 的“应用安装程序”。
主程序启动时会请求管理员权限，请同意 Windows UAC 提示。

若提示缺失 VCRUNTIME/MSVCP，请安装 Microsoft Visual C++ Redistributable x64：
https://aka.ms/vs/17/release/vc_redist.x64.exe

项目源码与反馈：
https://github.com/vusiif/zerotier-gui
https://gitee.com/vusiif/zerotier-gui

本包使用 Qt $qtVersion 动态库，许可证见 licenses。
Qt 官方源代码：https://download.qt.io/official_releases/qt/
Qt 第三方许可说明：https://doc.qt.io/qt-6/licenses-used-in-qt.html
本包不限制为调试修改 Qt 库而进行逆向工程，也不禁止替换兼容动态库。

winget 安装已由用户验证。自动测试使用模拟后端；实际联网、成员变更和服务重启后重连需要测试人员验证。
"@ | Set-Content -LiteralPath (Join-Path $staging 'README.txt') -Encoding utf8
if (Test-Path -LiteralPath (Join-Path $projectRoot 'TEST_REPORT.md')) { Copy-Item -LiteralPath (Join-Path $projectRoot 'TEST_REPORT.md') -Destination (Join-Path $staging 'TEST_REPORT.md') }
foreach ($name in @('zerotier_gui.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $staging $name))) { throw "Incomplete package: $name" }
}
Get-ChildItem -LiteralPath $staging -Recurse -File | ForEach-Object {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($_.FullName)
    try { $hash = [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '') }
    finally { $stream.Dispose(); $algorithm.Dispose() }
    [PSCustomObject]@{File = $_.FullName.Substring($staging.Length + 1); SHA256 = $hash }
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $staging 'manifest.json') -Encoding utf8
if (Test-Path -LiteralPath $destination) { Move-Item -LiteralPath $destination -Destination $backup }
Move-Item -LiteralPath $staging -Destination $destination
Write-Output "Published: $destination"
if (Test-Path -LiteralPath $backup) { Write-Output "Previous package retained: $backup" }
