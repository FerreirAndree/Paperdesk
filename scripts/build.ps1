[CmdletBinding()]
param([switch]$Test)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio Build Tools with Desktop development with C++.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'The MSVC C++ compiler is not installed.' }
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
# Import the compiler environment into this PowerShell process only.
$environment = & $env:ComSpec /d /c "call `"$devcmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not initialise the MSVC build environment.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
$common = @('/nologo', '/std:c++20', '/EHsc', '/W4', '/permissive-', '/utf-8', '/O2', '/MT', '/DUNICODE', '/D_UNICODE', '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN', '/D_WIN32_WINNT=0x0A00', "/I$root/src")
Push-Location $build
$env:VSLANG = '1033'
try {
    & rc /nologo /I "$root/src" /fo app.res "$root/src/app.rc"
    if ($LASTEXITCODE -ne 0) { throw 'Resource build failed.' }
    & cl @common "$root/src/model.cpp" "$root/src/storage.cpp" "$root/src/desktop.cpp" "$root/src/render.cpp" "$root/src/help.cpp" "$root/src/main.cpp" '/Fe:Paperdesk.exe' /link app.res /SUBSYSTEM:WINDOWS /MANIFEST:EMBED "/MANIFESTINPUT:$root/src/app.manifest" gdiplus.lib comctl32.lib user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib dwmapi.lib
    if ($LASTEXITCODE -ne 0) { throw 'Paperdesk build failed.' }
    if ($Test) {
        & cl @common "$root/src/model.cpp" "$root/src/storage.cpp" "$root/tests/core_tests.cpp" '/Fe:paperdesk_tests.exe' /link shell32.lib ole32.lib user32.lib
        if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
        & './paperdesk_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
        & cl @common "$root/src/model.cpp" "$root/src/render.cpp" "$root/tests/render_tests.cpp" '/Fe:paperdesk_render_tests.exe' /link app.res gdiplus.lib user32.lib gdi32.lib ole32.lib
        if ($LASTEXITCODE -ne 0) { throw 'Rendering test build failed.' }
        & './paperdesk_render_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Rendering tests failed.' }
        & cl @common "$root/src/desktop.cpp" "$root/tests/desktop_tests.cpp" '/Fe:paperdesk_desktop_tests.exe' /link user32.lib
        if ($LASTEXITCODE -ne 0) { throw 'Desktop test build failed.' }
        & './paperdesk_desktop_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Desktop tests failed.' }
    }
} finally { Pop-Location }
Write-Host "Built $build/Paperdesk.exe"
