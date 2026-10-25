Paperdesk 0.1.2 fixes notes disappearing after Win+D on the reported Windows setup.

Notes temporarily enter the topmost window band while Explorer's desktop has focus. Returning to an app removes that status and lowers the notes. The user confirmed that Win+D keeps notes visible with this build.

This release retains proportional resizing and launcher-only creation from 0.1.1. Drag the lower-right corner to scale the paper and its contents together. Use **New paper note** in your launcher or `Paperdesk.exe --new`. No global hotkeys are registered.

Validation includes 202 core checks, 62 rendering/layout checks, and 52 native window-layer checks. The verification report records remaining hardware and shell coverage.

Extract the ZIP and run `Paperdesk.exe`. To install and restore notes at sign-in, run `./scripts/install.ps1` from the extracted folder in PowerShell. `./scripts/uninstall.ps1` retains saved notes.

This remains a preview using documented Windows APIs to approximate desktop attachment. The note format remains version 2, compatible with 0.1.1. Version 1 files upgrade automatically when saved; keep a backup before downgrading to 0.1.0.

The executable is unsigned. A SHA-256 checksum accompanies the ZIP.
