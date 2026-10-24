[CmdletBinding()]
param([Parameter(Mandatory)][string]$Path, [string]$ExpectedObsPath)
$ErrorActionPreference = 'Stop'
$installer = New-Object -ComObject WindowsInstaller.Installer
$database = $null; $session = $null
function Read-Table([string]$table) {
    $view = $database.OpenView("SELECT * FROM $table")
    try {
        $null = $view.Execute()
        while ($record = $view.Fetch()) {
            try {
                $count = $record.GetType().InvokeMember('FieldCount', 'GetProperty', $null, $record, $null)
                $values = for ($i = 1; $i -le $count; $i++) { $record.GetType().InvokeMember('StringData', 'GetProperty', $null, $record, @($i)) }
                ,$values
            } finally { [Runtime.InteropServices.Marshal]::FinalReleaseComObject($record) | Out-Null }
        }
    } finally { $null = $view.Close(); [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) | Out-Null }
}
try {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $database = $installer.OpenDatabase($resolved, 0)
    $properties = @{}; foreach ($row in (Read-Table 'Property')) { $properties[$row[0]] = $row[1] }
    if ($properties.MSIRESTARTMANAGERCONTROL -ne 'Disable' -or $properties.REBOOT -ne 'ReallySuppress') { throw 'Unexpected automatic process/reboot policy.' }
    $closers = @(Read-Table 'Wix4CloseApplication')
    if ($closers.Count -ne 2) { throw 'Expected explicit interactive and silent process handling.' }
    foreach ($row in $closers) {
        $flags = [int]$row[4]
        if ($row[1] -ne 'obs64.exe' -or ($flags -band 62)) { throw 'Unexpected force/elevated/end-session/reboot behavior.' }
        if ($row[0] -eq 'CheckObsSilent' -and $flags -ne 0) { throw 'Silent install may not close OBS.' }
        if ($row[0] -eq 'CloseObsInteractive' -and $flags -ne 65) { throw 'Interactive close must prompt first and only send WM_CLOSE.' }
    }
    $sequence = @{}; foreach ($row in (Read-Table 'InstallExecuteSequence')) { $sequence[$row[0]] = [int]$row[2] }
    if ($sequence.Wix4CloseApplications_X64 -ge $sequence.ObsStillRunning -or $sequence.ObsStillRunning -ge $sequence.InstallFiles) { throw 'Process check must precede file installation.' }
    $files = @(Read-Table 'File')
    if ($files.Count -ne 5 -or @($files | Where-Object { $_[2] -match 'Qt6|\.pdb$' }).Count) { throw 'MSI must contain only plugin DLL, two locales, license and user guide.' }
    # OpenPackage + AppSearch only reads installation discovery. Never execute InstallFiles/ExecuteAction here.
    $session = $installer.OpenPackage($resolved, 1)
    if ($session.DoAction('AppSearch') -ne 1) { throw 'AppSearch failed.' }
    $found = $session.GetType().InvokeMember('Property', 'GetProperty', $null, $session, @('OBSEXE'))
    if ($ExpectedObsPath -and $found -ne $ExpectedObsPath) { throw "OBS discovery mismatch: '$found'" }
    Write-Host "PASS MSI payload, process policy, action order; OBS discovery: $found"
} finally {
    foreach ($object in @($session, $database, $installer)) { if ($null -ne $object) { [Runtime.InteropServices.Marshal]::FinalReleaseComObject($object) | Out-Null } }
}
