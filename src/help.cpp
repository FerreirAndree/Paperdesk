#include "help.h"
#include <algorithm>
#include <stdexcept>

namespace paper {
namespace {
HWND glossary = nullptr, previousFocus = nullptr;
HBRUSH background = nullptr;
HFONT heading = nullptr;
bool standalone = false, inspectable = false;
constexpr COLORREF CardColor = RGB(250, 251, 252);

void Shape(HWND window) {
    RECT bounds{}; GetWindowRect(window, &bounds);
    int radius = MulDiv(12, GetDpiForWindow(window), 96);
    HRGN region = CreateRoundRectRgn(0, 0, bounds.right - bounds.left + 1, bounds.bottom - bounds.top + 1, radius, radius);
    if (region && !SetWindowRgn(window, region, TRUE)) DeleteObject(region);
}

void Position(HWND window, HWND owner) {
    RECT anchor{};
    if (!owner || !GetWindowRect(owner, &anchor)) {
        POINT cursor{}; GetCursorPos(&cursor);
        anchor = {cursor.x, cursor.y, cursor.x, cursor.y};
    }
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &monitor);
    RECT bounds{}; GetWindowRect(window, &bounds);
    int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    const int gap = MulDiv(12, GetDpiForWindow(window), 96);
    int x = anchor.right + gap, y = anchor.top;
    if (x + width > monitor.rcWork.right) x = anchor.left - width - gap;
    if (!owner) {
        x = monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2;
        y = monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2;
    }
    x = std::clamp<LONG>(x, monitor.rcWork.left, std::max(monitor.rcWork.left, monitor.rcWork.right - width));
    y = std::clamp<LONG>(y, monitor.rcWork.top, std::max(monitor.rcWork.top, monitor.rcWork.bottom - height));
    SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}
void Close(HWND window) {
    HWND focus = previousFocus;
    HWND owner = GetWindow(window, GW_OWNER);
    DestroyWindow(window);
    // Restore the editor's caret only after an explicit dismissal.
    if (IsWindow(focus)) {
        PostMessageW(GetAncestor(focus, GA_ROOT), RestoreCommandHelpFocus, reinterpret_cast<WPARAM>(focus), 0);
    } else if (IsWindow(owner)) SetForegroundWindow(owner);
}
INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_INITDIALOG: {
        glossary = window;
        RECT client{}; GetClientRect(window, &client);
        SetWindowLongPtrW(window, GWL_STYLE, GetWindowLongPtrW(window, GWL_STYLE) & ~WS_CAPTION);
        SetWindowPos(window, nullptr, 0, 0, client.right, client.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (inspectable) SetWindowLongPtrW(window, GWL_EXSTYLE,
            (GetWindowLongPtrW(window, GWL_EXSTYLE) & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW);
        background = CreateSolidBrush(CardColor);
        LOGFONTW font{};
        auto normal = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
        GetObjectW(normal, sizeof(font), &font);
        font.lfWeight = FW_SEMIBOLD; font.lfHeight = MulDiv(font.lfHeight, 11, 9);
        heading = CreateFontIndirectW(&font);
        SendDlgItemMessageW(window, 1001, WM_SETFONT, reinterpret_cast<WPARAM>(heading), TRUE);
        Position(window, GetWindow(window, GW_OWNER));
        Shape(window);
        return TRUE;
    }
    case WM_CTLCOLORDLG: return reinterpret_cast<INT_PTR>(background);
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wp), GetDlgCtrlID(reinterpret_cast<HWND>(lp)) >= 2001 ? RGB(75, 85, 98) : RGB(31, 38, 46));
        SetBkColor(reinterpret_cast<HDC>(wp), CardColor);
        return reinterpret_cast<INT_PTR>(background);
    case WM_PAINT: {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
        RECT bounds{}; GetClientRect(window, &bounds);
        FillRect(dc, &bounds, background);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(222, 227, 233));
        auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        int radius = MulDiv(12, GetDpiForWindow(window), 96);
        RoundRect(dc, 0, 0, bounds.right, bounds.bottom, radius, radius);
        RECT divider{14, 30, 236, 30}; MapDialogRect(window, &divider);
        MoveToEx(dc, divider.left, divider.top, nullptr); LineTo(dc, divider.right, divider.top);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
        EndPaint(window, &paint); return TRUE;
    }
    case WM_DRAWITEM: {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (item->CtlID != IDCANCEL) break;
        FillRect(item->hDC, &item->rcItem, background);
        int x = (item->rcItem.left + item->rcItem.right) / 2, y = (item->rcItem.top + item->rcItem.bottom) / 2;
        int half = MulDiv(4, GetDpiForWindow(window), 96);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, MulDiv(1, GetDpiForWindow(window), 96)), RGB(92, 102, 113));
        auto old = SelectObject(item->hDC, pen);
        MoveToEx(item->hDC, x - half, y - half, nullptr); LineTo(item->hDC, x + half, y + half);
        MoveToEx(item->hDC, x - half, y + half, nullptr); LineTo(item->hDC, x + half, y - half);
        SelectObject(item->hDC, old); DeleteObject(pen);
        if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT)) {
            RECT focus = item->rcItem; InflateRect(&focus, -3, -3);
            HPEN outline = CreatePen(PS_SOLID, 1, RGB(168, 183, 201));
            auto oldPen = SelectObject(item->hDC, outline), oldBrush = SelectObject(item->hDC, GetStockObject(NULL_BRUSH));
            RoundRect(item->hDC, focus.left, focus.top, focus.right, focus.bottom, 7, 7);
            SelectObject(item->hDC, oldBrush); SelectObject(item->hDC, oldPen); DeleteObject(outline);
        }
        return TRUE;
    }
    case WM_SIZE: Shape(window); InvalidateRect(window, nullptr, TRUE); return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL || LOWORD(wp) == IDOK) { Close(window); return TRUE; }
        break;
    case WM_CLOSE: Close(window); return TRUE;
    case WM_HELP: return TRUE;
    case WM_NCHITTEST: {
        auto hit = DefWindowProcW(window, message, wp, lp);
        POINT point{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        ScreenToClient(window, &point);
        if (hit == HTCLIENT && point.y < MulDiv(52, GetDpiForWindow(window), 96)) {
            SetWindowLongPtrW(window, DWLP_MSGRESULT, HTCAPTION); return TRUE;
        }
        break;
    }
    case WM_DESTROY:
        glossary = nullptr; previousFocus = nullptr;
        if (heading) { DeleteObject(heading); heading = nullptr; }
        if (background) { DeleteObject(background); background = nullptr; }
        if (standalone) PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}
}
void SetCommandHelpTestMode(bool enabled) { inspectable = enabled; }
void ShowCommandHelp(HWND owner, HWND returnFocus) {
    if (IsWindow(glossary)) {
        // Windows may follow F1 with WM_HELP. Reopening the same card must not
        // discard the editor that requested it on the initial key event.
        if (returnFocus || GetWindow(glossary, GW_OWNER) != owner) previousFocus = returnFocus;
        SetWindowLongPtrW(glossary, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
        Position(glossary, owner); SetForegroundWindow(glossary); return;
    }
    previousFocus = returnFocus;
    HWND window = CreateDialogParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(103), owner, DialogProc, 0);
    if (!window) throw std::runtime_error("Could not open the command glossary");
    ShowWindow(window, SW_SHOWNORMAL);
    // A launcher's STARTF_USESHOWWINDOW/SW_HIDE can override the first ShowWindow.
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(window);
    SetFocus(window);
}
bool IsCommandHelpWindow(HWND window) { return glossary && (window == glossary || IsChild(glossary, window)); }
bool HandleCommandHelpMessage(MSG& message) { return IsCommandHelpWindow(message.hwnd) && IsDialogMessageW(glossary, &message); }
int RunCommandHelp() {
    standalone = true;
    ShowCommandHelp(nullptr);
    MSG message{};
    BOOL result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!HandleCommandHelpMessage(message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return result == -1 ? 1 : 0;
}
}
