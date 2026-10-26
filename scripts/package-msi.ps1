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
$spec = Get-Content -LiteralPath (Join-Path $root 'buildspec.json') -Raw | ConvertFrom-Json
$version = $spec.version
$stamp = Join-Path $root 'build_x64/generated/buildspec.sha256'
if (!(Test-Path $stamp) -or (Get-FileHash (Join-Path $root 'buildspec.json')).Hash -ne (Get-Content $stamp -Raw)) { throw 'Metadata changed; run scripts/build.ps1 before packaging.' }
if (-not (Select-String -LiteralPath (Join-Path $root 'build_x64\CMakeCache.txt') -Pattern "^CMAKE_PROJECT_VERSION:STATIC=$([regex]::Escape($version))$" -Quiet)) {
    throw 'Version changed since CMake configure. Run scripts/build.ps1 again.'
}
$output = Join-Path $root 'dist\installer'
New-Item -ItemType Directory -Path $output -Force | Out-Null
# MSI RichText requires RTF Unicode escapes; keep the original license text intact.
$notice = $spec.installer.welcomeText + "`n`n" + [IO.File]::ReadAllText((Join-Path $root 'installer/license-intro.txt')) + "`n`n"
$license = $notice + [IO.File]::ReadAllText((Join-Path $root 'LICENSE'))
$encoded = [Text.StringBuilder]::new()
foreach ($c in $license.ToCharArray()) {
    $n = [int]$c
    if ($c -eq "`r") { continue }
    if ($c -eq "`n") { [void]$encoded.Append('\par '); continue }
    if ($c -eq '\' -or $c -eq '{' -or $c -eq '}') { [void]$encoded.Append('\').Append($c); continue }
    if ($n -gt 127) { if ($n -gt 32767) { $n -= 65536 }; [void]$encoded.Append('\u').Append($n).Append('?'); continue }
    [void]$encoded.Append($c)
}
$rtf = Join-Path $output 'License.rtf'
[IO.File]::WriteAllText($rtf, '{\rtf1\ansi\uc1\deff0 {\fonttbl {\f0 Microsoft JhengHei;}}\f0\fs18 ' + $encoded.ToString() + '}', [Text.Encoding]::ASCII)
$suffix = if ($Preview) { '-unconfigured-preview' } else { '' }
$name = $spec.displayName + $(if ($Preview) { ' (Developer Preview)' } else { '' })
$localization = [xml]'<WixLocalization xmlns="http://wixtoolset.org/schemas/v4/wxl" Culture="zh-TW" />'
$strings = @{
    WelcomeDlg_Title = $spec.installer.title; WelcomeEulaDlg_Title = $spec.installer.title
    WelcomeDlgTitle = '{\WixUI_Font_Bigger}' + $spec.installer.welcome
    WelcomeDlgDescription = $spec.installer.welcomeText
    WelcomeEulaDlgTitle = '{\WixUI_Font_Title}' + $spec.installer.welcome
}
foreach ($key in $strings.Keys) {
    $entry = $localization.CreateElement('String', $localization.DocumentElement.NamespaceURI)
    $entry.SetAttribute('Id', $key); $entry.SetAttribute('Value', [string]$strings[$key])
    [void]$localization.DocumentElement.AppendChild($entry)
}
$localizationPath = Join-Path $output 'Product.zh-TW.wxl'; $localization.Save($localizationPath)
$wix = Join-Path $root '.tools\wix\cli\tools\net6.0\any\wix.dll'
$msi = Join-Path $output "twitch-category-assistant-$version-windows-x64$suffix.msi"
& $dotnet --roll-forward Major $wix build (Join-Path $root 'installer\Package.wxs') -arch x64 -culture zh-TW `
    -ext (Join-Path $root '.tools\wix\WixToolset.UI.wixext\wixext4\WixToolset.UI.wixext.dll') `
    -ext (Join-Path $root '.tools\wix\WixToolset.Util.wixext\wixext4\WixToolset.Util.wixext.dll') `
    -loc $localizationPath -d "Manufacturer=$($spec.author)" `
    -d "Payload=$payload" -d "ProjectRoot=$root" -d "ProductVersion=$version" -d "ProductName=$name" -d "LicenseRtf=$rtf" -o $msi
if ($LASTEXITCODE -ne 0) { throw 'WiX MSI build/validation failed.' }
# WiX 4 does not expose all SummaryInformation fields; set them before hashing/signing.
$installer = New-Object -ComObject WindowsInstaller.Installer
$summary = $null
try {
    $summary = $installer.SummaryInformation($msi, 6)
    $values = @{ 2=$spec.installer.title; 3=$spec.installer.subject; 4=$spec.author; 5=$spec.installer.keywords; 6=$spec.installer.comments }
    foreach ($key in $values.Keys) { $null = $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @([int]$key, [string]$values[$key])) }
    $summary.Persist()
} finally {
    if ($summary) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($summary) }
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)
}
$hash = (Get-FileHash -LiteralPath $msi -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$msi.sha256", "$hash  $([IO.Path]::GetFileName($msi))`n", [Text.Encoding]::ASCII)
Write-Host "MSI: $msi"
Write-Host 'Unsigned package. Verify installation/upgrade/removal on a test Windows machine before distribution.'
