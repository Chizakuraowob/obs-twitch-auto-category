[CmdletBinding()]
param([Parameter(Mandatory)][string]$Path, [string]$ExpectedObsPath, [switch]$ProbeLaunch, [switch]$ProbeProcess)
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
    $actions = @{}; foreach ($row in (Read-Table 'CustomAction')) { $actions[$row[0]] = $row }
    foreach ($name in @('CheckObsBeforeStart','CheckObsBeforeFiles')) { if ($actions[$name][3] -ne 'TcaCheckObs' -or $actions[$name][2] -ne 'ObsCheckBinary') { throw 'Missing native OBS process guard.' } }
    $sequence = @{}; foreach ($row in (Read-Table 'InstallExecuteSequence')) { $sequence[$row[0]] = [int]$row[2] }
    if ($sequence.CheckObsBeforeStart -ge $sequence.InstallValidate -or $sequence.CheckObsBeforeFiles -ge $sequence.InstallFiles -or $sequence.ContainsKey('Wix4CloseApplications_X64')) { throw 'Unexpected process check sequence.' }
    $uiSequence = @{}; foreach ($row in (Read-Table 'InstallUISequence')) { $uiSequence[$row[0]] = [int]$row[2] }
    if ($uiSequence.CheckObsBeforeStart -ge $uiSequence.ExecuteAction) { throw 'UI must check OBS before starting installation.' }
    $files = @(Read-Table 'File')
    if ($files.Count -ne 15 -or @($files | Where-Object { $_[2] -match 'Qt6|\.pdb$' }).Count) { throw 'Unexpected MSI payload; do not bundle OBS Qt runtime DLLs or PDBs.' }
    foreach ($doc in @('terms.md','privacy.md','contributors.md','third-party.md','source-distribution.md','WiX-MS-RL.txt','README.md')) {
        if (@($files | Where-Object { ($_[2] -split '\|')[-1] -eq $doc }).Count -ne 1) { throw "Missing public document: $doc" }
    }
    if (@($files | Where-Object { $_[2] -match 'data-processing\.md$' }).Count -ne 1) { throw 'MSI is missing the data-processing disclosure.' }
    if (@($files | Where-Object { $_[2] -match 'qschannelbackend\.dll$' }).Count -ne 1) { throw 'MSI is missing the HTTPS TLS backend.' }
    # OpenPackage + AppSearch only reads installation discovery. Never execute InstallFiles/ExecuteAction here.
    $session = $installer.OpenPackage($resolved, 1)
    if ($session.DoAction('AppSearch') -ne 1) { throw 'AppSearch failed.' }
    $found = $session.GetType().InvokeMember('Property', 'GetProperty', $null, $session, @('OBSEXE'))
    if ($ExpectedObsPath -and $found -ne $ExpectedObsPath) { throw "OBS discovery mismatch: '$found'" }
    $conditions = @(Read-Table 'LaunchCondition')
    foreach ($row in $conditions) {
        if ($session.EvaluateCondition($row[0]) -ne 1) { throw "Launch condition rejected this machine: $($row[1])" }
    }
    $osCondition = @($conditions | Where-Object { $_[1] -eq '需要 Windows 11 x64。' })[0][0]
    foreach ($case in @(@('603','9600','22000',1), @('603','9600','26300',1), @('1000','26300','19045',0), @('603','9600','',0), @('','9600','26300',0))) {
        for ($i=0; $i -lt 3; $i++) {
            $name = @('VersionNT64','WindowsBuild','TCA_WINDOWS_BUILD')[$i]
            $null = $session.GetType().InvokeMember('Property','SetProperty',$null,$session,@($name,$case[$i]))
        }
        if ($session.EvaluateCondition($osCondition) -ne $case[3]) { throw "Windows launch regression: $($case -join ',')" }
    }
    Write-Host "PASS MSI payload, process policy, action order; OBS discovery: $found"
} finally {
    foreach ($object in @($session, $database, $installer)) { if ($null -ne $object) { [Runtime.InteropServices.Marshal]::FinalReleaseComObject($object) | Out-Null } }
}
if ($ProbeLaunch -or $ProbeProcess) {
    # A disposable MSI stops immediately after real LaunchConditions, before any installation actions.
    $probeName = if ($ProbeProcess) { 'process-engine-probe' } else { 'launch-probe' }
    $probe = Join-Path (Split-Path -Parent $resolved) ($probeName + '.msi')
    $log = Join-Path (Split-Path -Parent $resolved) ($probeName + '.log')
    Copy-Item -LiteralPath $resolved -Destination $probe -Force
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $database = $installer.OpenDatabase($probe, 1)
    try {
        $actions = @{}; foreach ($row in (Read-Table 'InstallExecuteSequence')) { $actions[$row[0]] = [int]$row[2] }
        $stopAt = $actions.LaunchConditions + 1
        if ($ProbeProcess) {
            $guardAt = $actions.InstallValidate - 2; $stopAt = $actions.InstallValidate - 1
            if ($stopAt -ge $actions.RemoveExistingProducts) { throw 'Unsafe process probe sequence.' }
            $view = $database.OpenView("UPDATE InstallExecuteSequence SET Sequence=$guardAt WHERE Action='CheckObsBeforeStart'")
            try { $null = $view.Execute() } finally { $null = $view.Close(); [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) | Out-Null }
        }
        if ($stopAt -ge $actions.InstallInitialize) { throw 'Unsafe probe sequence.' }
        foreach ($sql in @(
            "INSERT INTO CustomAction (Action, Type, Target) VALUES ('TcaLaunchProbeStop', 19, 'TCA_LAUNCH_CHECK_OK')",
            "INSERT INTO InstallExecuteSequence (Action, Condition, Sequence) VALUES ('TcaLaunchProbeStop', '1', $stopAt)"
        )) {
            $view = $database.OpenView($sql)
            try { $null = $view.Execute() } finally { $null = $view.Close(); [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) | Out-Null }
        }
        $database.Commit()
    } finally {
        [Runtime.InteropServices.Marshal]::FinalReleaseComObject($database) | Out-Null
        [Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer) | Out-Null
    }
    $process = Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList @('/i', "`"$probe`"", '/qn', '/norestart', '/L*v', "`"$log`"") -Wait -PassThru -WindowStyle Hidden
    $content = Get-Content -LiteralPath $log -Raw
    if ($process.ExitCode -ne 1603 -or $content -match 'Doing action: (InstallInitialize|RemoveExistingProducts|InstallFiles)') { throw "Unsafe or failed real msiexec probe; inspect $log" }
    if ($ProbeProcess) {
        if ($content -notmatch 'TCA_OBS_CHECK: (clear|running)' -or ($content -match 'TCA_OBS_CHECK: clear' -and $content -notmatch 'Doing action: TcaLaunchProbeStop')) { throw "Process guard did not execute correctly; inspect $log" }
        Write-Host "PASS real msiexec process guard; stopped before installation; log: $log"
    } elseif ($content -notmatch 'Doing action: TcaLaunchProbeStop' -or $content -notmatch 'TCA_LAUNCH_CHECK_OK') { throw "Real msiexec launch check failed; inspect $log" }
    else { Write-Host "PASS real msiexec launch conditions; intentional pre-install stop ($($process.ExitCode)); log: $log" }
    Remove-Item -LiteralPath $probe
}
