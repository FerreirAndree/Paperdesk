Paperdesk 0.1.3 updates parchment, example notes, and sign-in startup.

- Parchment now uses an embedded texture with worn edges, age spots, fibres, and faint creases. The app decodes it once when first used. It works offline and requires no extra installed files.
- The README preview uses practical reminders: deadlines, build tasks, and trail notes. The welcome note also drops the motivational copy.
- Sign-in startup now uses a standard shortcut in the current user's Startup folder. Successful registration removes the legacy Run entry. Installation and removal check registration errors instead of silently continuing.
- A small last-run.log records whether the app started, loaded notes, and reached readiness. It contains no note text and is replaced on the next launch.
- The README explains how to resize individual text blocks: right-click directly on the text, then choose Text size.

Startup verification: Windows lists the new shortcut in its startup inventory. Launching that exact shortcut restored the saved note without changing its file; enable and disable were also checked. A real reboot/sign-in is still needed to confirm automatic launch on the reported machine.

Validation: 202 core and 62 rendering/layout checks passed locally. The existing desktop-layer test remains intermittent on this host; the Win+D implementation is unchanged from 0.1.2. See the verification report for details.

Extract the ZIP and run ./scripts/install.ps1 in PowerShell to install or update. Saved notes remain compatible with 0.1.1 and 0.1.2. The app is an unsigned preview; a SHA-256 checksum accompanies the download.
