[CmdletBinding()]
param([string]$Path)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$spec = Get-Content (Join-Path $root 'buildspec.json') -Raw | ConvertFrom-Json
if (!$Path) { $Path = Join-Path $root "dist/installer/twitch-category-assistant-$($spec.version)-windows-x64.msi" }
$dll = [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $root 'dist/twitch-category-assistant/bin/64bit/twitch-category-assistant.dll'))
foreach ($pair in @(@($dll.CompanyName,$spec.author), @($dll.FileDescription,$spec.description), @($dll.ProductName,$spec.displayName), @($dll.FileVersion,"$($spec.version).$($spec.revision)"), @($dll.ProductVersion,$spec.version), @($dll.LegalCopyright,$spec.copyright), @($dll.Comments,$spec.email))) {
    if ($pair[0] -cne $pair[1]) { throw "DLL metadata mismatch: '$($pair[0])' expected '$($pair[1])'" }
}
$installer = New-Object -ComObject WindowsInstaller.Installer
$summary = $null; $database = $null
try {
    $summary = $installer.SummaryInformation((Resolve-Path $Path).Path, 0)
    $expected = @{ 2=$spec.installer.title; 3=$spec.installer.subject; 4=$spec.author; 5=$spec.installer.keywords; 6=$spec.installer.comments }
    foreach ($key in $expected.Keys) {
        $value = $summary.GetType().InvokeMember('Property','GetProperty',$null,$summary,@([int]$key))
        if ($value -cne $expected[$key]) { throw "MSI summary field $key mismatch" }
    }
    $database = $installer.OpenDatabase((Resolve-Path $Path).Path, 0)
    foreach ($pair in @(@('ProductName',$spec.displayName), @('Manufacturer',$spec.author), @('ProductVersion',$spec.version))) {
        $view = $database.OpenView("SELECT Value FROM Property WHERE Property='$($pair[0])'")
        $record = $null
        try {
            $view.Execute(); $record = $view.Fetch()
            if (!$record -or $record.StringData(1) -cne $pair[1]) { throw "MSI property mismatch: $($pair[0])" }
        } finally { if ($record) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($record) }; $view.Close(); [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) }
    }
} finally {
    foreach ($obj in @($database,$summary,$installer)) { if ($obj) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($obj) } }
}
Add-Type -AssemblyName System.Windows.Forms
$rtf = New-Object System.Windows.Forms.RichTextBox
try {
    $rtf.Rtf = [IO.File]::ReadAllText((Join-Path $root 'dist/installer/License.rtf'))
    $intro = [IO.File]::ReadAllText((Join-Path $root 'installer/license-intro.txt')).Replace("`r",'').Trim()
    if (!$rtf.Text.Contains('GNU GENERAL PUBLIC LICENSE') -or !$rtf.Text.Contains($intro) -or !$rtf.Text.Contains($spec.installer.welcomeText)) { throw 'RTF Unicode/license round-trip failed' }
} finally { $rtf.Dispose() }
Write-Host 'PASS DLL version resource, MSI summary/properties, Unicode license text.'
