Paperdesk 0.1.5 prevents sign-in restoration from creating a welcome note.

The --restore command now restores saved notes only. A missing or intentionally empty profile stays empty. Running Paperdesk directly for the first time still shows the welcome note.

The launch log now records the backing storage directory. This helps distinguish the normal Windows profile from AppData redirected by a packaged development tool. A reported missing-note issue was traced to two separate profiles, and the user's saved note was recovered without changing its contents or placement. This release does not automatically merge profiles or change Windows' startup scheduling.

Regression checks cover missing profiles, first-launch welcome, unchanged saved notes after restoration, and intentionally empty desktops.

Install or update with ./scripts/install.ps1 from the release ZIP. Saved notes remain compatible with previous releases from 0.1.1 onward. The executable is an unsigned preview; a SHA-256 checksum accompanies the download.
