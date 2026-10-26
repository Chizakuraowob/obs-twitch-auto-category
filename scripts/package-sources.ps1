[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$spec = Get-Content -LiteralPath (Join-Path $root 'buildspec.json') -Raw | ConvertFrom-Json
$output = Join-Path $root 'dist/sources'
New-Item -ItemType Directory -Path $output -Force | Out-Null
# Archive the exact committed source, never caches, credentials, or an uncommitted working tree.
$changes = & git -C $root status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Commit the source changes before packaging corresponding source.' }
$commit = & git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine source commit.' }
& git -C $root archive --format=zip "--output=$output/twitch-category-assistant-$($spec.version)-source.zip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'Source archive failed.' }
$sources = @(
    @('qtbase-everywhere-src-6.11.1.zip', 'https://download.qt.io/archive/qt/6.11/6.11.1/submodules/qtbase-everywhere-src-6.11.1.zip', '3529cc37297a5a7aae4486843b9fd41c30df1d79a770f85e240b537dcc327ca5'),
    @('obs-deps-2026-07-15.zip', 'https://github.com/obsproject/obs-deps/archive/refs/tags/2026-07-15.zip', '682237d459e52d1608e5b9eacb3ce70aa3f152b787ce1570c5d6bbe10d2b92d7'),
    @('wix-4.0.6.zip', 'https://github.com/wixtoolset/wix/archive/refs/tags/v4.0.6.zip', 'e122c95b3c7c7082855d7a77eed595550f4f8cf6bdae294431eae57f0f454d97')
)
$manifest = @("Source commit: $commit", "Product version: $($spec.version)")
foreach ($source in $sources) {
    $file = Join-Path $output $source[0]
    if (!(Test-Path -LiteralPath $file)) { Invoke-WebRequest -Uri $source[1] -OutFile $file }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $source[2]) {
        throw "Source hash mismatch: $file. Do not release this archive."
    }
    $manifest += "$($source[2])  $($source[0])  $($source[1])"
}
$sbom = @(Get-ChildItem -LiteralPath (Join-Path $root '.deps/obs-deps-qt6-2026-07-15-x64') -Recurse -Filter 'qtbase*.spdx')
if ($sbom.Count -ne 1) { throw 'Expected exactly one QtBase SBOM in the configured Qt dependency.' }
Copy-Item -LiteralPath $sbom[0].FullName -Destination (Join-Path $output 'qtbase-6.11.1.spdx') -Force
foreach ($name in @("twitch-category-assistant-$($spec.version)-source.zip", 'qtbase-6.11.1.spdx')) {
    $manifest += "$((Get-FileHash -LiteralPath (Join-Path $output $name)).Hash.ToLowerInvariant())  $name"
}
[IO.File]::WriteAllLines((Join-Path $output 'SOURCE-MANIFEST.txt'), $manifest, [Text.UTF8Encoding]::new($false))
Write-Host "Corresponding source: $output (commit $commit)"
