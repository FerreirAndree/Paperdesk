# Architecture

## Process and windows

One event-driven process owns a hidden controller window and one layered `WS_POPUP` window per note. Idle note windows have `WS_EX_TOOLWINDOW` and `WS_EX_NOACTIVATE`. A per-profile, per-session named mutex prevents competing writers. Subsequent launches use bounded `WM_COPYDATA` messages to request a note or a clean shutdown.

The controller receives display changes, session shutdown, foreground WinEvents, and shell show/hide/reorder events. Notifications are delivered out of process; there is no injected hook DLL and no global hotkey registration. Foreground windows are resolved to their root before desktop classification. Eight delayed checks over roughly one second accommodate shell transitions. The timer then stops; there is no idle polling loop. Z-order is changed only when a note is on the wrong side of the target window.

## Desktop integration and limits

Windows does not publish an API for third-party wallpaper widgets. Reparenting into Explorer's `WorkerW` and the common `0x052C` shell-message technique rely on undocumented behaviour and are deliberately absent here.

Paperdesk uses `SetWindowPos` to lower notes when a different app gains focus. When Explorer's desktop gains focus, notes temporarily enter the topmost window band without activation. Selecting another application removes that status and lowers the notes. A stale desktop Z-order cannot trigger elevation while an application has focus. Editing a note preserves its current layer until focus leaves Paperdesk.

This is an approximation of desktop attachment. When the desktop is foreground, a note can overlap a visible, unfocused application window. Selecting that application lowers the notes again. Task View, virtual desktops, Explorer replacements, and Show desktop behaviour can vary across Windows versions. They need manual verification on the target Windows build. Notes are not guaranteed to be visible in every shell transition.

Startup registration creates a standard shell shortcut in the current user's Startup folder through IShellLink and IPersistFile. The shortcut launches the installed executable with --restore and no activation. A successful registration removes the old Paperdesk Run entry; disabling startup removes both registrations. Installation checks the registration process's exit code. Windows controls when startup apps launch; it may delay them after sign-in. Paperdesk cannot promise to draw before Explorer is ready or on the lock screen.

## Rendering and editing

GDI+ draws paper and text into premultiplied-alpha bitmaps for `UpdateLayeredWindow`. Sticky-paper fibres are deterministic and generated in code. Parchment uses an embedded PNG with transparent worn edges; its decoded pixel cache is shared by all parchment notes and has no GDI+ object lifetime beyond shutdown. See [asset provenance and prompt](assets.md). Redraws happen only on content, geometry, DPI, or feedback changes. Each note uses logical dimensions and physical desktop coordinates, allowing negative monitor positions. Missing monitors are handled by clamping notes into the nearest work area.

Text uses Segoe Print on sticky paper and Georgia on parchment. Windows supplies both fonts; font files are not redistributed. The renderer measures text before edits and style changes are accepted. Resizing changes a separate persisted zoom factor. Layout dimensions, text positions, font sizes, and wrapping widths remain unchanged; drawing and input coordinates use the product of display DPI and note zoom. The lower-right corner and Ctrl+Alt-drag both scale the note proportionally around its top-left window position.

A temporary native EDIT control handles typing, selection, paste, undo, and input methods. It grows within the available paper area. Both the native editor's metrics and the paper renderer's metrics must fit. Overflow restores the last accepted text; the control is reset to its first line. Saving removes the control and redraws the block as ink. Input is normalised to Windows newlines while editing.

## Quick reference

The quick reference is a modeless native popup owned by its note. Standard text controls expose the action labels to accessibility tools; custom painting supplies the rounded card and close icon. The card is excluded from the taskbar in normal use, positioned beside its owner, and clamped to the monitor work area. It has no timer or global key hook. F1 in the editor preserves the edit session; explicit dismissal posts focus restoration after the dialog has closed. The --help launcher runs the same card in a short-lived process without loading or changing notes.

## Storage

`model.cpp` owns the versioned UTF-8 format, limits and validation. `storage.cpp` writes to a same-directory temporary file, calls `FlushFileBuffers`, and commits with `ReplaceFileW`, retaining a backup. The first save uses `MoveFileExW` with write-through. A backup is read before a damaged original is renamed. An unreadable primary and backup stop loading instead of replacing user data with an empty state.

The current file format starts with `PAPERDESK 2`, followed by a note count. Each note stores a quoted ID, physical x/y, logical width/height, numeric style, display zoom, and block count. Each block stores x/y, font size, and quoted UTF-8 text. Quoted strings can contain newlines. Version 1 files load at 100% zoom and are upgraded on the next save. Do not edit the file while Paperdesk is running. Version 0.1.0 cannot read the newer format; retain a backup before downgrading.

For shell diagnostics, `--trace-desktop` enables a bounded log of foreground classes, note visibility, minimisation, DWM cloaking, topmost status, window-operation errors, and relative Z-order before and after placement. It does not log note text or other window titles. Logging stops after 2000 samples and is disabled on the next ordinary launch.

## API references

- [Window relationships and Z-order](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [Windows Startup folder](https://support.microsoft.com/en-au/windows/experience/startup-boot/configure-startup-applications-in-windows)
- [Shell links](https://learn.microsoft.com/en-us/windows/win32/shell/links)
