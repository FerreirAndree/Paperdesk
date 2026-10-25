Paperdesk 0.1.1 updates resizing, launcher behaviour, and desktop visibility.

- Drag the lower-right corner to scale a note like an image. Text, line breaks, and block positions stay in place. Ctrl+Alt-drag and right-click size actions are also available.
- Notes remember their display scale across restarts. Existing files load at 100% size.
- Ctrl+Alt+N has been removed. Paperdesk registers no global hotkeys. Use **New paper note** in your launcher or `Paperdesk.exe --new`.
- Desktop visibility now responds to shell stacking-order changes and retries briefly while a Show desktop transition settles. Notes are excluded from Peek fading.

Extract the ZIP and run `Paperdesk.exe`. For installation and startup restoration, run `./scripts/install.ps1` from the extracted folder in PowerShell. `./scripts/uninstall.ps1` removes the app while retaining notes.

This remains a preview. Notes use documented Windows window APIs to approximate desktop attachment. See the verification report for tested behaviour and remaining shell and hardware checks.

The Win+D repair still needs manual confirmation on the reported Windows setup; it is not claimed as verified in this release.

The note format is now version 2. Version 1 files are read automatically and upgraded on the next save. Keep a backup if you intend to downgrade to 0.1.0.

The release executable is unsigned. A SHA-256 checksum is included alongside the ZIP.
