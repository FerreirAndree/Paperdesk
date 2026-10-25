# Verification, 25 October 2026

## Version 0.1.1 follow-up

The updated build passed 202 core checks and 62 rendering/layout checks. New coverage includes reading version 1 notes at 100% scale, persisting the new zoom field, rejecting invalid scales, resize limits, and counting the actual rendered text lines at 35%, 50%, 72%, 100%, 150%, and 200% zoom. The small-size image test closes gaps within letters so detached dots are not mistaken for additional lines.

A live lower-right corner drag shrank a note to 71.93% while preserving its three text lines, paper dimensions, font size, and block coordinates in the saved file. The updated executable has no global hotkey registration or hotkey handler. Launcher creation remains available.

The user tested version 0.1.1 with Win+D and reported that notes still disappear. The trace showed visible, non-minimised, uncloaked notes below Explorer's desktop despite ordinary Z-order raises.

The follow-up candidate temporarily uses the topmost band while the desktop has focus and removes that status when an application gains focus. A new native integration test checks actual window styles, relative order, unchanged focus and geometry, and repeated elevation/demotion. Its windows are transparent and on-screen. The standalone run passed all 52 checks, alongside 202 core and 62 rendering checks. Initial runs immediately after compilation intermittently failed the elevation assertion; the standalone checks do not establish shell behaviour on every Windows build.

The user confirmed that Win+D keeps the notes visible with this candidate, released as 0.1.2. The diagnostic trace also recorded elevation on desktop focus and removal of topmost status when an app regained focus.

The measurements and initial interaction checks below describe version 0.1.0.

Test machine: Windows x64, build 26300.9711, 26H2. Built with MSVC 14.44 and Windows SDK 10.0.26100. The GUI checks used the developer `--test-ui` flag because the automation tool excludes ordinary tool windows from its inventory.

## Automated checks

The local release build passed 193 core checks and 50 rendering/layout checks. They cover UTF-8 and multiline round trips, negative monitor coordinates, truncated and malformed data, duplicate IDs, size limits, atomic replacement, backup recovery, preservation of damaged files, empty desktops, ignored uncommitted files, multiline layout, overflow, trailing newlines, transparent edges, and rendering at 100%, 125%, 150%, and 200% scaling.

GitHub Actions independently configured the CMake project, built both test executables and the app, passed CTest, and produced the ZIP package on Windows Server 2022. The initial successful run is [40197295054](https://github.com/FerreirAndree/Paperdesk/actions/runs/40197295054).

## Live interaction checks

Verified in the native app:

- Open an existing multiline block without losing line breaks.
- Ctrl+A and typing replace the selected text.
- A large paste is rejected and preserves the accepted text, with brief paper-full feedback.
- Ctrl+Enter commits and removes the editing field.
- Double-clicking lower on the paper creates a separate block at that position.
- The second block leaves the first block intact.
- Switching to parchment keeps both blocks and changes the paper and typeface.
- Escape restores the text from before the edit.
- Quitting and restarting restores saved text, positions, and paper style.
- Standard note windows are absent from the taskbar-window inventory; the inspection mode exposes them.

The visual preview in the README comes from the same renderer as the app.

## Resource sample

A release process with 20 notes used 32.46 MiB of working-set memory and 219 handles. Its measured CPU time did not increase during a 54.5-second idle sample. The executable is 426,496 bytes, approximately 417 KiB, including its icon and statically linked C++ runtime. These are measurements on this machine, not resource guarantees on every Windows build.

## Still needs hands-on coverage

Physical mixed-DPI monitors, monitor disconnect/reconnect, pen/IME input, sleep/wake, sign-out/sign-in, Explorer restart, Task View, virtual desktops, and extended Win+D stress testing remain unverified. The automated renderer checks at different scales do not substitute for physical DPI-transition tests. Modifier-drag gestures also remain on the manual checklist. Startup registration can be checked directly, but an actual sign-in cycle is required to confirm its timing.

Paperdesk is an initial preview. The documented shell limitations are deliberate consequences of using public window APIs.
