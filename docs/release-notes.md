Paperdesk puts paper notes on your Windows desktop with no dashboard, taskbar button, or permanent controls.

- Butter, rose, and parchment paper, rendered natively.
- Independently positioned text blocks with no scrolling.
- Per-note editing, moving, resizing, duplication, and deletion.
- Local autosave with atomic replacement and backup recovery.
- Launcher commands, Ctrl+Alt+N, and optional sign-in startup.
- A standalone x64 executable, about 417 KiB, with no runtime installation.

Extract the ZIP and run `Paperdesk.exe`. For installation and startup restoration, run `./scripts/install.ps1` from the extracted folder in PowerShell. `./scripts/uninstall.ps1` removes the app while retaining notes.

This is the first preview. Notes use documented Windows window APIs to approximate desktop attachment. Virtual desktops, repeated Show desktop transitions, physical mixed-DPI setups, and modifier-drag gestures still need hands-on testing. See the repository's verification report and architecture notes for tested behaviour and limitations.

The release executable is unsigned. A SHA-256 checksum is included alongside the ZIP.
