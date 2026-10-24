[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Join-Path (Split-Path -Parent $PSScriptRoot) '.tools\wix'
$packages = @(
    @{ Id='wix'; Folder='cli'; Archive='wix.4.0.6.zip'; Hash='a94dd42ae1fb56b32da180e2173ceda4f0d10b4c8871c5ee59ecb502131a1eb6' },
    @{ Id='WixToolset.UI.wixext'; Folder='WixToolset.UI.wixext'; Archive='WixToolset.UI.wixext.zip'; Hash='e92c4ddac5d17f5360291ab856c676e19fd92af14daffb5d5d4b8e1e9c716b47' },
    @{ Id='WixToolset.Util.wixext'; Folder='WixToolset.Util.wixext'; Archive='WixToolset.Util.wixext.zip'; Hash='541168d58c299dd8d62e92ea1877489756cfa73a27fd6ec855cd6341c8447562' }
)
New-Item -ItemType Directory -Path $root -Force | Out-Null
foreach ($package in $packages) {
    $archive = Join-Path $root $package.Archive
    if (-not (Test-Path -LiteralPath $archive)) {
        $id = $package.Id.ToLowerInvariant()
        Invoke-WebRequest -UseBasicParsing -Uri "https://api.nuget.org/v3-flatcontainer/$id/4.0.6/$id.4.0.6.nupkg" -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $package.Hash) { throw "SHA256 mismatch: $archive" }
    Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $root $package.Folder) -Force
}
Write-Host 'WiX 4.0.6 packaging tools ready in .tools/wix (no global installation).'
