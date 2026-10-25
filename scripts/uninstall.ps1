[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$install = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs/Paperdesk'))
$expected = [IO.Path]::GetFullPath((Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Programs/Paperdesk'))
if ($install -ne $expected) { throw 'The install directory is not the expected Paperdesk directory.' }
$executable = Join-Path $install 'Paperdesk.exe'
if (Test-Path -LiteralPath $executable) {
    Start-Process -FilePath $executable -ArgumentList '--startup','disable' -WindowStyle Hidden -Wait
    Start-Process -FilePath $executable -ArgumentList '--quit' -WindowStyle Hidden -Wait
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while (Get-Process Paperdesk -ErrorAction SilentlyContinue | Where-Object Path -eq $executable) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Paperdesk is still saving. Close it before uninstalling.' }
        Start-Sleep -Milliseconds 100
    }
}
foreach ($file in @('Paperdesk.exe', 'README.md', 'LICENSE')) {
    $path = Join-Path $install $file
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Paperdesk'
foreach ($name in @('New paper note.lnk', 'Paperdesk.lnk', 'Paperdesk help.lnk')) {
    $path = Join-Path $menu $name
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
Write-Host 'Paperdesk has been uninstalled. Your notes remain in %LOCALAPPDATA%/Paperdesk.'
