[CmdletBinding()]
param([switch]$NoStartup, [switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$source = Join-Path $root 'Paperdesk.exe'
if (-not (Test-Path -LiteralPath $source)) { $source = Join-Path $root 'build/Paperdesk.exe' }
if (-not (Test-Path -LiteralPath $source)) { throw 'Build Paperdesk first with ./scripts/build.ps1, or extract a release ZIP.' }
$install = Join-Path $env:LOCALAPPDATA 'Programs/Paperdesk'
$target = Join-Path $install 'Paperdesk.exe'
New-Item -ItemType Directory -Path $install -Force | Out-Null
if (Test-Path -LiteralPath $target) {
    Start-Process -FilePath $target -ArgumentList '--quit' -WindowStyle Hidden -Wait
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while (Get-Process Paperdesk -ErrorAction SilentlyContinue | Where-Object Path -eq $target) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Paperdesk could not finish saving. Close it before installing.' }
        Start-Sleep -Milliseconds 100
    }
}
Copy-Item -LiteralPath $source -Destination $target -Force
foreach ($file in @('README.md', 'LICENSE')) {
    if (Test-Path -LiteralPath (Join-Path $root $file)) { Copy-Item -LiteralPath (Join-Path $root $file) -Destination $install -Force }
}
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Paperdesk'
New-Item -ItemType Directory -Path $menu -Force | Out-Null
$shell = New-Object -ComObject WScript.Shell
foreach ($item in @(@('New paper note', '--new'), @('Paperdesk', '--restore'), @('Paperdesk help', '--help'))) {
    $shortcut = $shell.CreateShortcut((Join-Path $menu ($item[0] + '.lnk')))
    $shortcut.TargetPath = $target
    $shortcut.Arguments = $item[1]
    $shortcut.WorkingDirectory = $install
    $shortcut.Description = 'Paper notes for your desktop'
    $shortcut.Save()
}
if (-not $NoStartup) { Start-Process -FilePath $target -ArgumentList '--startup','enable' -WindowStyle Hidden -Wait }
if (-not $NoLaunch) { Start-Process -FilePath $target -ArgumentList '--restore' -WindowStyle Hidden }
Write-Host "Installed to $target"
Write-Host 'Launch New paper note from Start or your launcher. Ctrl+Alt+N also creates a note.'
