[CmdletBinding()]
param([switch]$Preview)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dotnet = Join-Path $env:ProgramFiles 'dotnet\dotnet.exe'
if (-not (Test-Path -LiteralPath $dotnet)) { throw 'Packaging requires Microsoft .NET 8 Runtime x64 or newer. End users do not need it.' }
$header = Join-Path $root 'build_x64\generated\release-config.hpp'
if (-not (Test-Path -LiteralPath $header)) { throw 'Run scripts/build.ps1 first.' }
$configured = (Get-Content -LiteralPath $header -Raw) -match 'PublisherClientId\[\]\s*=\s*"[a-zA-Z0-9]+"'
if (-not $configured -and -not $Preview) { throw 'Publisher Client ID is missing. Build with -ClientId first, or use -Preview for an explicitly unconfigured developer MSI.' }
$payload = Join-Path $root 'dist\twitch-category-assistant'
$builtDll = Join-Path $root 'build_x64\RelWithDebInfo\twitch-category-assistant.dll'
$stagedDll = Join-Path $payload 'bin\64bit\twitch-category-assistant.dll'
if (-not (Test-Path -LiteralPath $stagedDll)) { throw 'Build and stage the plugin first.' }
if ((Get-Item -LiteralPath $header).LastWriteTimeUtc -gt (Get-Item -LiteralPath $builtDll).LastWriteTimeUtc -or
    (Get-FileHash -LiteralPath $builtDll).Hash -ne (Get-FileHash -LiteralPath $stagedDll).Hash) { throw 'Build/staging is stale; run scripts/build.ps1 again.' }
& (Join-Path $PSScriptRoot 'setup-packaging.ps1')
$version = (Get-Content -LiteralPath (Join-Path $root 'buildspec.json') -Raw | ConvertFrom-Json).version
if (-not (Select-String -LiteralPath (Join-Path $root 'build_x64\CMakeCache.txt') -Pattern "^CMAKE_PROJECT_VERSION:STATIC=$([regex]::Escape($version))$" -Quiet)) {
    throw 'Version changed since CMake configure. Run scripts/build.ps1 again.'
}
$output = Join-Path $root 'dist\installer'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$license = (Get-Content -LiteralPath (Join-Path $root 'LICENSE') -Raw).Replace('\', '\\').Replace('{', '\{').Replace('}', '\}') -replace '\r?\n', '\par '
$rtf = Join-Path $output 'License.rtf'
[IO.File]::WriteAllText($rtf, '{\rtf1\ansi\deff0 {\fonttbl {\f0 Segoe UI;}}\f0\fs18 ' + $license + '}', [Text.Encoding]::ASCII)
$suffix = if ($Preview) { '-unconfigured-preview' } else { '' }
$name = if ($Preview) { 'Twitch Category Assistant (Developer Preview)' } else { 'Twitch Category Assistant' }
$wix = Join-Path $root '.tools\wix\cli\tools\net6.0\any\wix.dll'
$msi = Join-Path $output "twitch-category-assistant-$version-windows-x64$suffix.msi"
& $dotnet --roll-forward Major $wix build (Join-Path $root 'installer\Package.wxs') -arch x64 -culture zh-TW `
    -ext (Join-Path $root '.tools\wix\WixToolset.UI.wixext\wixext4\WixToolset.UI.wixext.dll') `
    -ext (Join-Path $root '.tools\wix\WixToolset.Util.wixext\wixext4\WixToolset.Util.wixext.dll') `
    -d "Payload=$payload" -d "ProjectRoot=$root" -d "ProductVersion=$version" -d "ProductName=$name" -d "LicenseRtf=$rtf" -o $msi
if ($LASTEXITCODE -ne 0) { throw 'WiX MSI build/validation failed.' }
$hash = (Get-FileHash -LiteralPath $msi -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$msi.sha256", "$hash  $([IO.Path]::GetFileName($msi))`n", [Text.Encoding]::ASCII)
Write-Host "MSI: $msi"
Write-Host 'Unsigned package. Verify installation/upgrade/removal on a test Windows machine before distribution.'
