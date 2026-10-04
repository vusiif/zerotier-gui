param(
    [string]$QtSource = '.docs-tmp/qt-static/qtbase-everywhere-src-6.11.2',
    [int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
if ($Jobs -lt 1 -or $Jobs -gt 32) { throw 'Jobs must be between 1 and 32.' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = [IO.Path]::GetFullPath((Join-Path $root $QtSource))
$kit = Join-Path $root '.docs-tmp/qt-static/install'
$qtBuild = Join-Path $root '.docs-tmp/qt-static/build'
$appBuild = Join-Path $root 'cmake-build-static'
$cmake = Join-Path $env:LOCALAPPDATA 'Programs/CLion/bin/cmake/win/x64/bin/cmake.exe'
$ninja = Join-Path $env:LOCALAPPDATA 'Programs/CLion/bin/ninja/win/x64/ninja.exe'
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat'
foreach ($file in @($cmake, $ninja, $vs, (Join-Path $source 'CMakeLists.txt'))) {
    if (-not (Test-Path -LiteralPath $file)) { throw "Missing build input: $file" }
}
New-Item -ItemType Directory (Join-Path $root '.docs-tmp/qt-static') -Force | Out-Null
$driver = Join-Path $root '.docs-tmp/qt-static/build-static.cmd'
@"
@echo off
call "$vs" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b 1
set VSLANG=1033
"$cmake" -S "$source" -B "$qtBuild" -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$kit" -DBUILD_SHARED_LIBS=OFF -DFEATURE_static_runtime=ON -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_BENCHMARKS=OFF -DFEATURE_sql=OFF -DFEATURE_printsupport=OFF -DFEATURE_openssl=OFF -DFEATURE_vulkan=OFF -DFEATURE_dbus=OFF -DFEATURE_icu=OFF -DFEATURE_system_zlib=OFF -DFEATURE_system_png=OFF -DFEATURE_system_jpeg=OFF -DFEATURE_system_freetype=OFF -DFEATURE_system_pcre2=OFF
if errorlevel 1 exit /b 1
"$cmake" --build "$qtBuild" -j $Jobs
if errorlevel 1 exit /b 1
"$cmake" --install "$qtBuild"
if errorlevel 1 exit /b 1
"$cmake" -S "$root" -B "$appBuild" -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="$kit/lib/cmake/Qt6" -DCMAKE_PREFIX_PATH="$kit" -DZEROTIER_GUI_STATIC=ON -DZEROTIER_GUI_BUILD_TESTS=ON
if errorlevel 1 exit /b 1
"$cmake" --build "$appBuild" -j $Jobs -- -d keeprsp
if errorlevel 1 exit /b 1
"$(Join-Path (Split-Path $cmake -Parent) 'ctest.exe')" --test-dir "$appBuild" --output-on-failure
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $driver -Encoding ascii
& $env:ComSpec /c $driver
if ($LASTEXITCODE -ne 0) { throw "Static build failed: $LASTEXITCODE" }
Write-Output "Static executable: $(Join-Path $appBuild 'zerotier_gui.exe')"
