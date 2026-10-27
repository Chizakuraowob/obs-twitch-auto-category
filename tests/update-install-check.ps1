$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../src/update/install-update.ps1')
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('tca-update-check-' + [guid]::NewGuid())
[void](New-Item -ItemType Directory -Path $temporary)
$script:launched = 0
$script:exitCode = 0
$script:running = $false
$script:parentRunning = $false
$script:canExit = $true
$script:parentPath = 'C:\OBS\obs64.exe'
function Get-Process {
    param($Id, $Name, $ErrorAction)
    if ($Id -and $script:parentRunning) {
        $fake = [pscustomobject]@{ Path = $script:parentPath; Handle = 1 }
        $fake | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { param($timeout) return $script:canExit }
        return $fake
    }
    if ($Name -and $script:running) { return [pscustomobject]@{ Id = 123 } }
}
function Start-Process {
    param($FilePath, $ArgumentList, [switch]$PassThru, [switch]$Wait)
    if ($FilePath -notlike '*msiexec.exe' -or $ArgumentList -notlike '*/norestart*' -or !$Wait) { throw 'Invalid installer invocation' }
    $expected = '/i "' + $script:expectedInstaller + '" /norestart /L*v "' + $script:expectedInstaller + '.install.log"'
    if ($ArgumentList -cne $expected) { throw 'MSI and log paths must use native Windows separators and remain quoted.' }
    ++$script:launched
    return [pscustomobject]@{ ExitCode = $script:exitCode }
}
try {
    $known = Join-Path $temporary 'known-hash.txt'
    [IO.File]::WriteAllText($known, 'abc')
    if ((Get-UpdateDigest $known) -ine 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') { throw 'Incorrect SHA-256' }
    foreach ($case in @('success', 'reboot', 'cancel', 'failure', 'hash', 'other-obs', 'close-cancelled', 'closed-parent', 'wrong-parent')) {
        $file = Join-Path $temporary ('update ' + $case + '.msi')
        [IO.File]::WriteAllText($file, 'not a real MSI - never executed')
        $digest = Get-UpdateDigest $file
        $script:exitCode = switch ($case) { 'reboot' { 3010 }; 'cancel' { 1602 }; 'failure' { 1603 }; default { 0 } }
        $script:running = $case -eq 'other-obs'
        $script:parentRunning = $case -in @('close-cancelled', 'closed-parent', 'wrong-parent')
        $script:parentPath = if ($case -eq 'wrong-parent') { 'C:\Other\obs64.exe' } else { 'C:\OBS\obs64.exe' }
        $script:canExit = $case -ne 'close-cancelled'
        if ($case -eq 'hash') { [IO.File]::WriteAllText($file, 'tampered') }
        $script:launched = 0; $failed = $false
        $script:expectedInstaller = [IO.Path]::GetFullPath($file).Replace('/', '\')
        try { Invoke-VerifiedUpdate -ObsProcessId 1 -ObsExecutable 'C:/OBS/obs64.exe' -Installer $file.Replace('\', '/') -Digest $digest } catch { $failed = $true }
        $success = $case -in @('success', 'reboot', 'closed-parent')
        $expectedLaunch = if ($case -in @('hash', 'other-obs', 'close-cancelled', 'wrong-parent')) { 0 } else { 1 }
        if ($failed -eq $success -or (Test-Path -LiteralPath $file) -eq $success -or $script:launched -ne $expectedLaunch) { throw "Failed case: $case" }
    }
    $eventName = 'Local\TCA-Update-Test-' + [guid]::NewGuid()
    $event = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset, $eventName)
    $proceedName = $eventName + '-continue'
    $proceed = [Threading.EventWaitHandle]::new($true, [Threading.EventResetMode]::ManualReset, $proceedName)
    try {
        $script:parentRunning = $false; $script:running = $true; $script:launched = 0
        try { Invoke-VerifiedUpdate -ObsProcessId 1 -ObsExecutable 'C:\OBS\obs64.exe' -Installer $known -Digest ('a' * 64) -ReadyEvent $eventName -ProceedEvent $proceedName } catch {}
        if ($event.WaitOne(0)) { throw 'Invalid parameters acknowledged' }
        try { Invoke-VerifiedUpdate -ObsProcessId 1 -ObsExecutable 'C:\OBS\obs64.exe' -Installer (Join-Path $temporary 'update wrong-parent.msi') -Digest ('a' * 64) -ReadyEvent $eventName -ProceedEvent ($proceedName + '-missing') } catch {}
        if ($event.WaitOne(0) -or $script:launched) { throw 'Missing permission event must not acknowledge or launch' }
        try { Invoke-VerifiedUpdate -ObsProcessId 1 -ObsExecutable 'C:\OBS\obs64.exe' -Installer (Join-Path $temporary 'update wrong-parent.msi') -Digest ('a' * 64) -ReadyEvent $eventName -ProceedEvent $proceedName } catch {}
        if (!$event.WaitOne(0) -or $script:launched) { throw 'Readiness handshake failed' }
        [void]$event.Reset(); [void]$proceed.Reset(); $script:running = $false; $message = ''
        try { Invoke-VerifiedUpdate -ObsProcessId 1 -ObsExecutable 'C:\OBS\obs64.exe' -Installer (Join-Path $temporary 'update wrong-parent.msi') -Digest ('a' * 64) -ReadyEvent $eventName -ProceedEvent $proceedName } catch { $message = $_.Exception.Message }
        if (!$event.WaitOne(0) -or $script:launched -or $message -notlike '*not started*') { throw 'Timed-out handshake must not launch' }
    } finally { $event.Dispose(); $proceed.Dispose() }
    Write-Output 'Update handoff: 9 scenarios, SHA-256 and readiness handshake passed; no OBS closed or MSI executed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $expectedParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if ([IO.Path]::GetDirectoryName($resolved) -ine $expectedParent -or [IO.Path]::GetFileName($resolved) -notlike 'tca-update-check-*') { throw 'Unsafe fixture cleanup path' }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
