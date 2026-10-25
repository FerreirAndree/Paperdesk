#include "desktop.h"
#include <iostream>
#include <stdexcept>
using namespace paper;
static int checks = 0;
void Settle() {
    const ULONGLONG end = GetTickCount64() + 80;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    } while (GetTickCount64() < end);
}
void Check(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
int main() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.hInstance = instance; wc.lpfnWndProc = DefWindowProcW; wc.lpszClassName = L"Paperdesk.LayerTest";
    if (!RegisterClassW(&wc)) return 1;
    // Transparent, test-owned windows on the primary screen.
    // Never activate them or alter Explorer.
    HWND note = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 10, 10, 100, 100, nullptr, nullptr, instance, nullptr);
    HWND other = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 10, 10, 100, 100, nullptr, nullptr, instance, nullptr);
    SetLayeredWindowAttributes(note, 0, 0, LWA_ALPHA); SetLayeredWindowAttributes(other, 0, 0, LWA_ALPHA);
    HWND focus = GetForegroundWindow();
    int result = 0;
    try {
        Check(note && other, "Could not create layer test windows");
        RECT before{}; GetWindowRect(note, &before);
        ShowWindow(note, SW_SHOWNOACTIVATE);
        ShowWindow(other, SW_SHOWNOACTIVATE);
        Settle();
        Check(PlaceOnDesktop(note, other), "Desktop elevation failed");
        Settle();
        Check((GetWindowLongPtrW(note, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0, "Note did not enter the desktop elevation layer");
        Check(IsAbove(note, other), "Note stayed behind the covering window");
        Check(GetForegroundWindow() == focus, "Desktop elevation stole focus");
        SetWindowPos(other, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        Check(!IsAbove(note, other), "The other topmost test window was not above the note");
        Check(PlaceBehindApps(note, other), "Background demotion failed");
        Check(!(GetWindowLongPtrW(note, GWL_EXSTYLE) & WS_EX_TOPMOST), "Note remained topmost behind another topmost app");
        Check(!IsAbove(note, other), "Note still covers the app window");
        Check(GetForegroundWindow() == focus, "Background demotion stole focus");
        RECT after{}; GetWindowRect(note, &after);
        Check(EqualRect(&before, &after), "Layer transitions moved or resized the note");
        for (int i = 0; i < 10; ++i) {
            Check(PlaceOnDesktop(note, other), "Repeated elevation failed");
            Check((GetWindowLongPtrW(note, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0, "Repeated elevation did not apply");
            Check(PlaceBehindApps(note, other), "Repeated demotion failed");
            Check(!(GetWindowLongPtrW(note, GWL_EXSTYLE) & WS_EX_TOPMOST), "Repeated demotion did not apply");
        }
        Check(!(GetWindowLongPtrW(note, GWL_EXSTYLE) & WS_EX_TOPMOST), "Repeated switching left the note topmost");
        std::cout << checks << " desktop layer checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    if (note) DestroyWindow(note);
    if (other) DestroyWindow(other);
    UnregisterClassW(wc.lpszClassName, instance);
    return result;
}
