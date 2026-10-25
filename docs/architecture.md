# Architecture

## Process and windows

One event-driven process owns a hidden controller window and one layered `WS_POPUP` window per note. Idle note windows have `WS_EX_TOOLWINDOW` and `WS_EX_NOACTIVATE`. A per-profile, per-session named mutex prevents competing writers. Subsequent launches use bounded `WM_COPYDATA` messages to request a note or a clean shutdown.

The controller receives display changes, session shutdown, a global hotkey, and a foreground WinEvent hook. Foreground notifications are delivered out of process; there is no injected hook DLL. A single delayed event after a foreground change accommodates shell transitions. There is no recurring desktop polling timer.

## Desktop integration and limits

Windows does not publish an API for third-party wallpaper widgets. Reparenting into Explorer's `WorkerW` and the common `0x052C` shell-message technique rely on undocumented behaviour and are deliberately absent here.

Paperdesk uses `SetWindowPos` to lower notes when a different app gains focus. If the desktop shell gains focus, notes are shown without activation and raised. They never use the topmost window band. This also lets the app recover from the shell bringing its desktop window forward.

This is an approximation of desktop attachment. When the desktop is foreground, a note can overlap a visible, unfocused application window. Selecting that application lowers the notes again. Task View, virtual desktops, Explorer replacements, and Show desktop behaviour can vary across Windows versions. They need manual verification on the target Windows build. Notes are not guaranteed to be visible in every shell transition.

Startup registration uses the current user's documented Run key. Windows controls when startup apps launch; it may delay them after sign-in. Paperdesk cannot promise to draw before Explorer is ready or on the lock screen.

## Rendering and editing

GDI+ draws paper and text into premultiplied-alpha bitmaps for `UpdateLayeredWindow`. Fibres are deterministic and generated in code. Redraws happen only on content, geometry, DPI, or feedback changes. Each note uses logical dimensions and physical desktop coordinates, allowing negative monitor positions. Missing monitors are handled by clamping notes into the nearest work area.

Text uses Segoe Print on sticky paper and Georgia on parchment. Windows supplies both fonts; font files are not redistributed. The renderer measures text before edits, resizes, and style changes are accepted.

A temporary native EDIT control handles typing, selection, paste, undo, and input methods. It grows within the available paper area. Both the native editor's metrics and the paper renderer's metrics must fit. Overflow restores the last accepted text; the control is reset to its first line. Saving removes the control and redraws the block as ink. Input is normalised to Windows newlines while editing.

## Storage

`model.cpp` owns the versioned UTF-8 format, limits and validation. `storage.cpp` writes to a same-directory temporary file, calls `FlushFileBuffers`, and commits with `ReplaceFileW`, retaining a backup. The first save uses `MoveFileExW` with write-through. A backup is read before a damaged original is renamed. An unreadable primary and backup stop loading instead of replacing user data with an empty state.

The file format starts with `PAPERDESK 1`, followed by a note count. Each note stores a quoted ID, physical x/y, logical width/height, numeric style, and block count. Each block stores x/y, font size, and quoted UTF-8 text. Quoted strings can contain newlines. The format is intentionally small and versioned. Do not edit it while Paperdesk is running.

## API references

- [Window relationships and Z-order](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [Run and RunOnce registry keys](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys)
