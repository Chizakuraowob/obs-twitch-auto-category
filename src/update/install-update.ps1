param(
    [int]$ObsProcessId,
    [string]$ObsExecutable,
    [string]$Installer,
    [string]$Digest,
    [string]$ReadyEvent
)

function Get-UpdateDigest([string]$Path) {
    $hash = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '') }
    finally { $stream.Dispose(); $hash.Dispose() }
}

function Invoke-VerifiedUpdate {
    param([int]$ObsProcessId, [string]$ObsExecutable, [string]$Installer, [string]$Digest, [string]$ReadyEvent)
    $ErrorActionPreference = 'Stop'
    if ($ObsProcessId -le 0 -or $Digest -notmatch '^[a-fA-F0-9]{64}$' -or [IO.Path]::GetExtension($Installer) -ine '.msi') {
        throw 'Invalid update parameters. The installer has been kept.'
    }
    $process = Get-Process -Id $ObsProcessId -ErrorAction SilentlyContinue
    if ($process) {
        if ($process.Path -ine $ObsExecutable) { throw 'OBS process changed. The installer has been kept.' }
        $null = $process.Handle
    }
    if ($ReadyEvent) {
        $ready = [Threading.EventWaitHandle]::OpenExisting($ReadyEvent)
        try { [void]$ready.Set() } finally { $ready.Dispose() }
    }
    if ($process -and !$process.WaitForExit(120000)) { throw 'OBS did not close within two minutes. The installer has been kept.' }
    if (Get-Process -Name obs64, obs32 -ErrorAction SilentlyContinue) { throw 'Another OBS instance is still running. Close OBS and retry the installer.' }
    if ((Get-UpdateDigest $Installer) -ine $Digest) { throw 'SHA-256 verification failed. Installation was cancelled.' }
    $log = $Installer + '.install.log'
    $msiexec = Join-Path $env:SystemRoot 'System32/msiexec.exe'
    # Keep the MSI wizard and normal Windows elevation/security prompts visible.
    $arguments = '/i "' + $Installer + '" /norestart /L*v "' + $log + '"'
    $install = Start-Process -FilePath $msiexec -ArgumentList $arguments -PassThru -Wait
    if ($install.ExitCode -notin @(0, 1641, 3010)) { throw "Installation did not complete (code $($install.ExitCode)). MSI and log kept: $log" }
    Remove-Item -LiteralPath $Installer -ErrorAction Stop
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        Invoke-VerifiedUpdate -ObsProcessId $ObsProcessId -ObsExecutable $ObsExecutable -Installer $Installer -Digest $Digest -ReadyEvent $ReadyEvent
    }
    catch {
        $message = $_.Exception.Message
        $message | Out-File -LiteralPath ($Installer + '.update-error.log') -Encoding utf8
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show($message, 'Twitch Category Assistant - Update', 'OK', 'Warning')
        exit 1
    }
}
