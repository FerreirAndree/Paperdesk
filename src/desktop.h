#pragma once
#include <windows.h>

namespace paper {
bool IsAbove(HWND window, HWND other);
bool PlaceOnDesktop(HWND window, HWND desktop);
bool PlaceBehindApps(HWND window, HWND foreground);
}
