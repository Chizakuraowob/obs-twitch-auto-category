$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $root ('build_x64/source-package-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$fixture/scripts", "$fixture/dist/sources" -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'scripts/package-sources.ps1') -Destination "$fixture/scripts/package-sources.ps1"
[IO.File]::WriteAllText("$fixture/buildspec.json", '{"version":"0.0.1"}')
[IO.File]::WriteAllText("$fixture/.gitignore", "/dist/`n")
& git -C $fixture init -q
if ($LASTEXITCODE) { throw 'Fixture git init failed.' }
& git -C $fixture add .
if ($LASTEXITCODE) { throw 'Fixture git add failed.' }
& git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid commit -qm fixture
if ($LASTEXITCODE) { throw 'Fixture git commit failed.' }
# A corrupt cached archive must fail before any network request or release manifest.
[IO.File]::WriteAllText("$fixture/dist/sources/qtbase-everywhere-src-6.11.1.zip", 'corrupt archive')
try {
    & "$fixture/scripts/package-sources.ps1"
    throw 'Corrupt source was accepted.'
} catch {
    if ($_.Exception.Message -notlike 'Source hash mismatch:*') { throw }
}
if (Test-Path -LiteralPath "$fixture/dist/sources/SOURCE-MANIFEST.txt") { throw 'A failed source bundle must not have a manifest.' }
[IO.File]::WriteAllText("$fixture/buildspec.json", '{"version":"0.0.2"}')
try {
    & "$fixture/scripts/package-sources.ps1"
    throw 'Uncommitted source was accepted.'
} catch {
    if ($_.Exception.Message -notlike 'Commit the source changes*') { throw }
}
Write-Host 'PASS source packaging rejects corrupt archives and uncommitted source (isolated fixture, no network).'
