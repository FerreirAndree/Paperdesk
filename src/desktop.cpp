#include "desktop.h"

namespace paper {
bool IsAbove(HWND window, HWND other) {
    for (HWND current = GetTopWindow(nullptr); current; current = GetWindow(current, GW_HWNDNEXT)) {
        if (current == window) return true;
        if (current == other) return false;
    }
    return false;
}
bool PlaceOnDesktop(HWND window, HWND desktop) {
    if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) && IsAbove(window, desktop) && IsWindowVisible(window)) return true;
    return SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW) != FALSE;
}
bool PlaceBehindApps(HWND window, HWND foreground) {
    // Clear topmost even if another topmost app is already above the note.
    if (!(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) && !IsAbove(window, foreground)) return true;
    return SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE) != FALSE;
}
}
