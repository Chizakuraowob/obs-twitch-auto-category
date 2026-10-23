[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
if (Get-Process -Name obs64 -ErrorAction SilentlyContinue) { throw 'OBS is running. Close OBS before installing the plugin.' }
$sourceDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\twitch-category-assistant'
$destinationDirectory = Join-Path $env:ProgramData 'obs-studio\plugins\twitch-category-assistant'
if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory 'bin\64bit\twitch-category-assistant.dll'))) { throw 'Build and package the plugin first.' }
if (Test-Path -LiteralPath $destinationDirectory) { throw 'Destination already exists. Back it up and review before replacing an installed plugin.' }
New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceDirectory 'bin') -Destination $destinationDirectory -Recurse
Copy-Item -LiteralPath (Join-Path $sourceDirectory 'data') -Destination $destinationDirectory -Recurse
Write-Output "Plugin installed at $destinationDirectory. OBS was not started."
