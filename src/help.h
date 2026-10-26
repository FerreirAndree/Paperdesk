#pragma once
#include <windows.h>

namespace paper {
constexpr UINT RestoreCommandHelpFocus = WM_APP + 20;
void SetCommandHelpTestMode(bool enabled);
void ShowCommandHelp(HWND owner, HWND returnFocus = nullptr);
bool IsCommandHelpWindow(HWND window);
bool HandleCommandHelpMessage(MSG& message);
int RunCommandHelp();
}
