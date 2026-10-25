[CmdletBinding()]
param([string]$Executable = '', [string]$Version = '0.1.0')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Executable) { $Executable = Join-Path $root 'build/Paperdesk.exe' }
if ($Version -notmatch '^\d+\.\d+\.\d+([.-][a-zA-Z0-9]+)*$') { throw 'Invalid version.' }
$folder = Join-Path $root "dist/Paperdesk-$Version-windows-x64"
New-Item -ItemType Directory -Path (Join-Path $folder 'scripts') -Force | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $folder 'Paperdesk.exe') -Force
foreach ($file in @('README.md', 'LICENSE', 'CONTRIBUTING.md')) { Copy-Item -LiteralPath (Join-Path $root $file) -Destination $folder -Force }
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $folder -Recurse -Force
foreach ($file in @('install.ps1', 'uninstall.ps1')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $folder 'scripts') -Force }
$zip = "$folder.zip"
Compress-Archive -Path "$folder/*" -DestinationPath $zip -Force
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zip.sha256" -Value "$hash  $([IO.Path]::GetFileName($zip))" -Encoding ascii
Write-Host $zip
