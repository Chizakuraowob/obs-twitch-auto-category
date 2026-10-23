[CmdletBinding()]
param([switch]$ConfigureOnly)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2026 Build Tools first; see docs/toolchain-setup.md.' }
$installation = & $vswhere -latest -products '*' -version '[18.0,19.0)' -property installationPath
if (-not $installation) { throw 'Visual Studio 2026 Build Tools not found.' }
$cmake = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
Push-Location $projectDirectory
try {
    & $cmake --preset windows-x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    if ($ConfigureOnly) { return }
    & $cmake --build --preset windows-x64 --parallel
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
    $savedPath = $env:PATH
    try {
        $env:PATH = (Join-Path $projectDirectory '.deps\obs-deps-qt6-2026-07-15-x64\bin') + ';' + $savedPath
        & $ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    } finally { $env:PATH = $savedPath }
    & $cmake --install build_x64 --config RelWithDebInfo --prefix (Join-Path $projectDirectory 'dist')
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
} finally { Pop-Location }
