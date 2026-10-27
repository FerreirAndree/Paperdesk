[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$folder = Join-Path ([IO.Path]::GetTempPath()) ('Paperdesk-startup-' + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $folder | Out-Null
$script:resident = $null
function Start-Notes([string]$Command) {
    $log = Join-Path $folder 'last-run.log'
    if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log }
    $script:resident = Start-Process -FilePath $Executable -ArgumentList "$Command --data-dir `"$folder`"" -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        if ((Test-Path -LiteralPath $log) -and ((Get-Content -LiteralPath $log -Raw) -match 'UTC ready')) { return }
        if ($script:resident.HasExited) { throw 'Paperdesk exited before becoming ready.' }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Paperdesk did not become ready.'
}
function Stop-Notes {
    if ($script:resident -and -not $script:resident.HasExited) {
        $quit = Start-Process -FilePath $Executable -ArgumentList "--quit --data-dir `"$folder`"" -WindowStyle Hidden -PassThru
        if (-not $quit.WaitForExit(5000) -or -not $script:resident.WaitForExit(5000)) { throw 'Paperdesk did not close.' }
    }
    $script:resident = $null
}
try {
    Start-Notes '--restore'
    Stop-Notes
    $notes = Join-Path $folder 'notes.paper'
    if (Test-Path -LiteralPath $notes) { throw 'Restore created a welcome note in a missing profile.' }

    Start-Notes ''
    Stop-Notes
    if ((Get-Content -LiteralPath $notes -Raw) -notmatch 'Your desktop') { throw 'An explicit first launch lost its welcome note.' }

    $original = (Get-FileHash -LiteralPath $notes).Hash
    Start-Notes '--restore'
    Stop-Notes
    if ((Get-FileHash -LiteralPath $notes).Hash -ne $original) { throw 'Restore modified the saved notes.' }

    Set-Content -LiteralPath $notes -Value "PAPERDESK 2`n0" -Encoding ascii
    Start-Notes '--restore'
    Stop-Notes
    if ((Get-Content -LiteralPath $notes -Raw) -match 'Your desktop') { throw 'Restore replaced an intentionally empty desktop.' }
    Write-Host '4 startup checks passed'
} finally {
    Stop-Notes
    # Only remove the unique test-owned directory created above.
    if ((Split-Path $folder -Leaf) -match '^Paperdesk-startup-[0-9a-f-]{36}$') {
        Remove-Item -LiteralPath $folder -Recurse -Force
    }
}
