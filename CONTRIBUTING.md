# Contributing

Keep Paperdesk small. New idle controls, resident services, network dependencies, or a dashboard change the premise of the app. Discuss them before implementation.

Build with `./scripts/build.ps1 -Test`, or CMake and CTest as described in the README. Tests must run in a unique temporary directory, never in the user's notes folder. Keep the storage format backwards compatible or add a migration with recovery tests.

## Manual checks

Use an isolated profile:

```powershell
./build/Paperdesk.exe --data-dir build/manual-profile --no-hotkey
```

The developer-only `--test-ui` flag adds taskbar visibility so automation tools can discover the windows. It changes window flags, so repeat shell behaviour checks without it. `--render-preview docs/paperdesk.png` generates the README illustration through the actual renderer.

- Create, edit, cancel, paste, undo, and finish multiline text. Check Unicode, IME, and selection replacement.
- Fill a note. Typing or pasting more must preserve accepted text without scrolling or clipping.
- Place several text blocks; move one without moving the others.
- Move and resize a note. A resize must not hide existing text.
- Switch each paper style and text size, including a change that would overflow.
- Duplicate, delete, and undo delete. Delete the last note, restart, and verify it stays gone.
- Quit and restart. Text, paper style, size, and position must survive.
- Open another application over a note, return to the desktop, and use Show desktop twice.
- Check Alt+Tab, Task View, virtual desktops, sleep/wake, and an Explorer restart.
- Move between monitors at different scaling levels; disconnect a monitor and verify notes remain reachable.
- Install, sign out/in, disable startup, and uninstall while retaining note data.

Record what was actually tested and the Windows build. Do not describe implemented multi-monitor or shell handling as verified without running those scenarios.
