[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$installerPath = Join-Path $env:TEMP 'tca-vs2026-buildtools.exe'
Invoke-WebRequest -Uri 'https://aka.ms/vs/18/stable/vs_buildtools.exe' -OutFile $installerPath
$signature = Get-AuthenticodeSignature -LiteralPath $installerPath
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw 'Installer does not have a valid Microsoft signature.'
}
$arguments = @('--quiet', '--wait', '--norestart', '--nocache',
    '--add', 'Microsoft.VisualStudio.Workload.VCTools', '--includeRecommended',
    '--add', 'Microsoft.VisualStudio.Component.Windows11SDK.26100',
    '--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project')
$process = Start-Process -FilePath $installerPath -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -eq 3010) { Write-Output 'Installation completed; Windows restart required.' }
elseif ($process.ExitCode -ne 0) { throw "Installer returned $($process.ExitCode)." }
else { Write-Output 'Build Tools installation completed.' }
