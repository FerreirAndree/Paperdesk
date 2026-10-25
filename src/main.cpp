#include "model.h"
#include "storage.h"
#include "render.h"
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>

using namespace paper;
namespace {
constexpr wchar_t NoteClass[] = L"Paperdesk.Note";
constexpr wchar_t EditorClass[] = L"Paperdesk.Editor";
constexpr UINT SaveTimer = 1, DesktopTimer = 2, FullTimer = 3;
constexpr UINT DesktopEvent = WM_APP + 1, FinishEditor = WM_APP + 2, ExecuteCommand = WM_APP + 3;
constexpr ULONG_PTR IpcMagic = 0x50415052;
struct Options {
    std::filesystem::path directory = DefaultDirectory();
    bool create = false, quit = false, help = false, noHotkey = false, testUI = false;
    Style style = Style::Butter;
    std::wstring text, preview, startup;
};
Options Parse(const wchar_t* command) {
    int count = 0;
    LPWSTR* args = CommandLineToArgvW(command, &count);
    if (!args) throw std::runtime_error("Could not read command line");
    Options o;
    try {
        for (int i = 1; i < count; ++i) {
            std::wstring a = args[i];
            auto value = [&]() -> std::wstring {
                if (++i >= count) throw std::runtime_error("A command line value is missing");
                return args[i];
            };
            if (a == L"--new") o.create = true;
            else if (a == L"--style") o.style = ParseStyle(value());
            else if (a == L"--text") o.text = value();
            else if (a == L"--data-dir") o.directory = value();
            else if (a == L"--quit") o.quit = true;
            else if (a == L"--restore") {}
            else if (a == L"--no-hotkey") o.noHotkey = true;
            else if (a == L"--test-ui") o.testUI = true;
            else if (a == L"--help" || a == L"-h") o.help = true;
            else if (a == L"--render-preview") o.preview = value();
            else if (a == L"--startup") o.startup = value();
            else throw std::runtime_error("Unknown command line option");
        }
    } catch (...) { LocalFree(args); throw; }
    LocalFree(args);
    if (o.text.size() > MaxText) throw std::runtime_error("Text exceeds 4096 characters");
    if (!o.startup.empty() && o.startup != L"enable" && o.startup != L"disable") throw std::runtime_error("Use --startup enable or --startup disable");
    return o;
}
void Error(HWND owner, const std::exception& e) { MessageBoxW(owner, Wide(e.what()).c_str(), L"Paperdesk", MB_OK | MB_ICONERROR); }
void Help(HWND owner) {
    MessageBoxW(owner,
        L"A little paper, right on your desktop.\n\n"
        L"Double-click paper    Write a new text block\n"
        L"Double-click text        Edit that block\n"
        L"Alt + drag                     Move the paper\n"
        L"Alt + Shift + drag       Move a text block\n"
        L"Ctrl + Alt + drag         Resize the paper\n"
        L"Right-click                   Paper, text size, delete, help\n\n"
        L"While writing: Ctrl+Enter saves; Escape cancels.\n"
        L"Click away to finish. Text never scrolls off the paper.\n\n"
        L"Ctrl+Alt+N creates a new note from any app.\n"
        L"Launcher command: Paperdesk.exe --new\n"
        L"Optional: --style butter|rose|parchment --text \"Remember this\"\n\n"
        L"Deleted a note? Right-click another and choose Undo delete.\n"
        L"With no notes left, run Paperdesk.exe --new.\n\n"
        L"Paperdesk.exe --quit closes the app and keeps your notes.\n"
        L"Paperdesk.exe --startup enable|disable controls sign-in startup.",
        L"Paperdesk / How to use", MB_OK | MB_ICONINFORMATION);
}
std::wstring Executable() {
    std::wstring path(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) throw std::runtime_error("Could not locate Paperdesk.exe");
    path.resize(length);
    return path;
}
void Startup(bool enable) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) throw std::runtime_error("Could not open sign-in startup settings");
    LSTATUS status;
    if (enable) {
        std::wstring command = L"\"" + Executable() + L"\" --restore";
        if (command.size() > 259) { RegCloseKey(key); throw std::runtime_error("Install Paperdesk at a shorter path before enabling startup"); }
        status = RegSetValueExW(key, L"Paperdesk", 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, L"Paperdesk");
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) throw std::runtime_error("Could not update sign-in startup settings");
}
struct App;
struct NoteView {
    App* app;
    Note note;
    HWND window = nullptr;
    float scale = 1;
    enum class Drag { None, Paper, Block, Resize } drag = Drag::None;
    POINT dragStart{};
    Note beforeDrag;
    int dragBlock = -1;
    bool full = false;
    NoteView(App* owner, Note value) : app(owner), note(std::move(value)) {}
    void Open();
    void Paint();
    void Place(bool clamp = true);
    int Hit(float x, float y) const;
    void Edit(float x, float y, int block = -1);
    void Menu(POINT screen);
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
};
struct Editor {
    App* app;
    NoteView* view;
    int index;
    bool added = false, changing = false, ending = false;
    Block original;
    std::wstring accepted;
    HWND window = nullptr, input = nullptr;
    HFONT font = nullptr;
    HBRUSH brush = nullptr;
    WNDPROC inputProc = nullptr;
    Editor(App* a, NoteView* v, int i, bool isNew) : app(a), view(v), index(i), added(isNew), original(v->note.blocks[i]), accepted(WindowsNewlines(original.text)) {}
    void Open();
    void Change();
    void Position();
    void Finish(bool cancel);
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK InputProc(HWND, UINT, WPARAM, LPARAM);
    ~Editor() { if (font) DeleteObject(font); if (brush) DeleteObject(brush); }
};
struct App {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    Store store;
    HWND control = nullptr;
    std::vector<std::unique_ptr<NoteView>> notes;
    std::unique_ptr<Editor> editor;
    std::vector<Note> deleted;
    std::deque<Options> commands;
    bool dirty = false, saveErrorShown = false, shuttingDown = false, testUI = false;
    HWINEVENTHOOK foregroundHook = nullptr;
    explicit App(const Options& options) : store(options.directory), testUI(options.testUI) {}
    ~App() { if (foregroundHook) UnhookWinEvent(foregroundHook); }
    State Snapshot() const {
        State state;
        for (const auto& view : notes) state.notes.push_back(view->note);
        return state;
    }
    void Dirty() { dirty = true; SetTimer(control, SaveTimer, 350, nullptr); }
    bool Save() {
        KillTimer(control, SaveTimer);
        if (!dirty) return true;
        try { store.Save(Snapshot()); dirty = false; saveErrorShown = false; return true; }
        catch (const std::exception& e) {
            if (!saveErrorShown) { saveErrorShown = true; Error(control, e); }
            SetTimer(control, SaveTimer, 3000, nullptr);
            return false;
        }
    }
    void EndEdit(bool cancel = false) {
        if (!editor || editor->ending) return;
        editor->Finish(cancel);
        editor.reset();
    }
    void Add(Note note) {
        auto view = std::make_unique<NoteView>(this, std::move(note));
        view->Open();
        notes.push_back(std::move(view));
    }
    void New(Style style, const std::wstring& text, bool edit = true) {
        EndEdit();
        if (notes.size() >= MaxNotes) { MessageBoxW(control, L"There are already 64 notes. Delete one before adding another.", L"Paperdesk", MB_OK); return; }
        Note note;
        note.id = NewId(); note.style = style;
        POINT pointer{}; GetCursorPos(&pointer);
        note.x = pointer.x - 150; note.y = pointer.y - 90;
        if (!text.empty()) {
            note.blocks.push_back({32, 43, 22, text});
            if (!AllFit(note)) { MessageBoxW(control, L"That text does not fit on a new note. Create a blank note, resize it, then add your text.", L"Paperdesk", MB_OK | MB_ICONINFORMATION); return; }
        }
        Add(std::move(note)); Dirty(); Save();
        auto* view = notes.back().get();
        ShowWindow(view->window, SW_SHOWNOACTIVATE);
        if (edit && text.empty()) view->Edit(32, 43);
        else SetWindowPos(view->window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    void Remove(NoteView* view) {
        EndEdit();
        deleted.push_back(view->note);
        if (deleted.size() > MaxNotes) deleted.erase(deleted.begin());
        DestroyWindow(view->window);
        std::erase_if(notes, [=](const auto& p) { return p.get() == view; });
        Dirty(); Save();
    }
    void UndoDelete() {
        if (deleted.empty() || notes.size() >= MaxNotes) return;
        Add(deleted.back()); deleted.pop_back(); Dirty(); Save();
    }
    void Layer();
    void RunCommand(const Options& o) {
        if (o.quit) { PostMessageW(control, WM_CLOSE, 0, 0); return; }
        if (o.create) New(o.style, o.text);
        else Layer();
    }
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
};
App* running = nullptr;
bool IsDesktop(HWND hwnd) {
    if (!hwnd) return false;
    wchar_t name[80]{};
    GetClassNameW(hwnd, name, 80);
    return wcscmp(name, L"Progman") == 0 || wcscmp(name, L"WorkerW") == 0 || hwnd == GetShellWindow();
}
void CALLBACK Foreground(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    if (running) PostMessageW(running->control, DesktopEvent, 0, 0);
}
void App::Layer() {
    HWND foreground = GetForegroundWindow();
    DWORD pid = 0; GetWindowThreadProcessId(foreground, &pid);
    if (pid == GetCurrentProcessId()) return;
    bool desktop = IsDesktop(foreground);
    for (auto& v : notes) {
        if (desktop) ShowWindow(v->window, SW_SHOWNOACTIVATE);
        SetWindowPos(v->window, desktop ? HWND_TOP : HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    }
}
void NoteView::Paint() {
    int hidden = app->editor && app->editor->view == this ? app->editor->index : -1;
    if (!Present(window, note, scale, hidden, full)) {
        OutputDebugStringW(L"Paperdesk: failed to present a note.\n");
    }
}
void NoteView::Place(bool clamp) {
    if (clamp) {
        RECT bounds{note.x, note.y, note.x + static_cast<LONG>((note.width + Shadow * 2) * scale), note.y + static_cast<LONG>((note.height + Shadow * 2) * scale)};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor)) {
            int w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
            note.x = std::clamp(note.x, static_cast<int>(monitor.rcWork.left), std::max(static_cast<int>(monitor.rcWork.left), static_cast<int>(monitor.rcWork.right) - w));
            note.y = std::clamp(note.y, static_cast<int>(monitor.rcWork.top), std::max(static_cast<int>(monitor.rcWork.top), static_cast<int>(monitor.rcWork.bottom) - h));
        }
    }
    SetWindowPos(window, nullptr, note.x, note.y, static_cast<int>(std::ceil((note.width + Shadow * 2) * scale)), static_cast<int>(std::ceil((note.height + Shadow * 2) * scale)), SWP_NOACTIVATE | SWP_NOZORDER);
    Paint();
}
void NoteView::Open() {
    std::wstring title = L"Paperdesk / " + std::wstring(StyleName(note.style));
    DWORD exStyle = WS_EX_LAYERED | (app->testUI ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
    window = CreateWindowExW(exStyle, NoteClass, title.c_str(), WS_POPUP, note.x, note.y, 1, 1, nullptr, nullptr, app->instance, this);
    if (!window) throw std::runtime_error("Could not create a paper window");
    scale = GetDpiForWindow(window) / 96.f;
    Place();
    ShowWindow(window, SW_SHOWNOACTIVATE);
    SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}
int NoteView::Hit(float x, float y) const {
    for (int i = static_cast<int>(note.blocks.size()) - 1; i >= 0; --i) {
        auto r = BlockBounds(note, note.blocks[i]);
        r.Inflate(5, 5);
        if (r.Contains(x, y)) return i;
    }
    return -1;
}
void NoteView::Edit(float x, float y, int block) {
    app->EndEdit();
    if (block < 0) block = Hit(x, y);
    bool added = block < 0;
    if (added) {
        if (note.blocks.size() >= MaxBlocks) { MessageBeep(MB_OK); return; }
        Block b;
        b.x = std::clamp(x, Margin, note.width - Margin - 70);
        b.y = std::clamp(y, Margin, note.height - Margin - 45);
        if (!Fits(note, b)) return;
        note.blocks.push_back(b);
        block = static_cast<int>(note.blocks.size()) - 1;
    }
    app->editor = std::make_unique<Editor>(app, this, block, added);
    app->editor->Open();
    Paint();
}
void NoteView::Menu(POINT screen) {
    app->EndEdit();
    POINT local = screen; ScreenToClient(window, &local);
    float x = local.x / scale - Shadow, y = local.y / scale - Shadow;
    int block = Hit(x, y);
    HMENU menu = CreatePopupMenu(), styles = CreatePopupMenu(), sizes = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, block >= 0 ? L"Edit text\tDouble-click" : L"Write here\tDouble-click");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < 3; ++i) AppendMenuW(styles, MF_STRING | (static_cast<int>(note.style) == i ? MF_CHECKED : 0), 10 + i, StyleName(static_cast<Style>(i)));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(styles), L"Paper");
    AppendMenuW(sizes, MF_STRING, 20, L"Small"); AppendMenuW(sizes, MF_STRING, 21, L"Medium"); AppendMenuW(sizes, MF_STRING, 22, L"Large");
    AppendMenuW(menu, MF_POPUP | (block < 0 ? MF_GRAYED : 0), reinterpret_cast<UINT_PTR>(sizes), L"Text size");
    if (block >= 0) AppendMenuW(menu, MF_STRING, 23, L"Remove this text");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 30, L"New note\tCtrl+Alt+N");
    AppendMenuW(menu, MF_STRING, 31, L"Duplicate note");
    AppendMenuW(menu, MF_STRING, 32, L"Delete note");
    AppendMenuW(menu, MF_STRING | (app->deleted.empty() ? MF_GRAYED : 0), 33, L"Undo delete");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 40, L"How to use");
    AppendMenuW(menu, MF_STRING, 41, L"Quit Paperdesk");
    SetForegroundWindow(window);
    UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, window, nullptr);
    DestroyMenu(menu);
    PostMessageW(window, WM_NULL, 0, 0);
    if (choice == 1) Edit(x, y, block);
    else if (choice >= 10 && choice <= 12) {
        auto old = note.style;
        note.style = static_cast<Style>(choice - 10);
        if (!AllFit(note)) { note.style = old; full = true; SetTimer(window, FullTimer, 2200, nullptr); }
        SetWindowTextW(window, (L"Paperdesk / " + std::wstring(StyleName(note.style))).c_str());
        app->Dirty(); Paint();
    } else if (choice >= 20 && choice <= 22 && block >= 0) {
        float old = note.blocks[block].size;
        note.blocks[block].size = choice == 20 ? 17.f : choice == 21 ? 22.f : 29.f;
        if (!AllFit(note)) { note.blocks[block].size = old; full = true; SetTimer(window, FullTimer, 2200, nullptr); }
        app->Dirty(); Paint();
    } else if (choice == 23 && block >= 0) { note.blocks.erase(note.blocks.begin() + block); app->Dirty(); Paint(); }
    else if (choice == 30) app->New(note.style, L"");
    else if (choice == 31 && app->notes.size() < MaxNotes) {
        Note copy = note; copy.id = NewId(); copy.x += 36; copy.y += 36;
        app->Add(copy); app->Dirty();
    } else if (choice == 32) { app->Remove(this); return; }
    else if (choice == 33) app->UndoDelete();
    else if (choice == 40) Help(window);
    else if (choice == 41) PostMessageW(app->control, WM_CLOSE, 0, 0);
    if (!app->editor) app->Save();
}
LRESULT CALLBACK NoteView::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* v = reinterpret_cast<NoteView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        v = static_cast<NoteView*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        v->window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(v));
    }
    if (!v) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ScreenToClient(hwnd, &p);
        float x = p.x / v->scale - Shadow, y = p.y / v->scale - Shadow;
        if (x < 0 || y < 0 || x > v->note.width || y > v->note.height - 7) return HTTRANSPARENT;
        return HTCLIENT;
    }
    case WM_LBUTTONDBLCLK:
        v->Edit(GET_X_LPARAM(lp) / v->scale - Shadow, GET_Y_LPARAM(lp) / v->scale - Shadow); return 0;
    case WM_LBUTTONDOWN: {
        if (!(GetKeyState(VK_MENU) & 0x8000)) return 0;
        v->app->EndEdit();
        v->beforeDrag = v->note; GetCursorPos(&v->dragStart);
        v->dragBlock = v->Hit(GET_X_LPARAM(lp) / v->scale - Shadow, GET_Y_LPARAM(lp) / v->scale - Shadow);
        if (GetKeyState(VK_CONTROL) & 0x8000) v->drag = Drag::Resize;
        else if ((GetKeyState(VK_SHIFT) & 0x8000) && v->dragBlock >= 0) v->drag = Drag::Block;
        else v->drag = Drag::Paper;
        SetCapture(hwnd); return 0;
    }
    case WM_MOUSEMOVE: {
        if (v->drag == Drag::None) return 0;
        POINT p{}; GetCursorPos(&p);
        float dx = (p.x - v->dragStart.x) / v->scale, dy = (p.y - v->dragStart.y) / v->scale;
        if (v->drag == Drag::Paper) {
            v->note.x = v->beforeDrag.x + p.x - v->dragStart.x;
            v->note.y = v->beforeDrag.y + p.y - v->dragStart.y;
            SetWindowPos(hwnd, nullptr, v->note.x, v->note.y, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
        } else {
            auto candidate = v->note;
            if (v->drag == Drag::Resize) {
                candidate.width = std::clamp(v->beforeDrag.width + dx, MinWidth, MaxSize);
                candidate.height = std::clamp(v->beforeDrag.height + dy, MinHeight, MaxSize);
            } else {
                auto& b = candidate.blocks[v->dragBlock];
                const auto& old = v->beforeDrag.blocks[v->dragBlock];
                b.x = std::clamp(old.x + dx, Margin, candidate.width - Margin - 16);
                b.y = std::clamp(old.y + dy, Margin, candidate.height - Margin - 16);
            }
            if (AllFit(candidate)) { v->note = std::move(candidate); v->Place(false); }
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (v->drag != Drag::None) { v->drag = Drag::None; ReleaseCapture(); v->Place(); v->app->Dirty(); v->app->Save(); }
        return 0;
    case WM_CAPTURECHANGED:
        if (v->drag != Drag::None) { v->drag = Drag::None; v->Place(); v->app->Dirty(); }
        return 0;
    case WM_CONTEXTMENU: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (p.x == -1 && p.y == -1) { p = {v->note.x + 60, v->note.y + 60}; }
        v->Menu(p); return 0;
    }
    case WM_DPICHANGED:
        v->app->EndEdit();
        v->scale = HIWORD(wp) / 96.f;
        v->note.x = reinterpret_cast<RECT*>(lp)->left; v->note.y = reinterpret_cast<RECT*>(lp)->top;
        v->Place();
        if (v->drag != Drag::None) { v->beforeDrag = v->note; GetCursorPos(&v->dragStart); }
        v->app->Dirty(); return 0;
    case WM_TIMER:
        if (wp == FullTimer) { KillTimer(hwnd, FullTimer); v->full = false; v->Paint(); } return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, (GetKeyState(VK_MENU) & 0x8000) ? ((GetKeyState(VK_CONTROL) & 0x8000) ? IDC_SIZENWSE : IDC_SIZEALL) : IDC_ARROW)); return TRUE;
    case WM_CLOSE: return 0; // Closing the process is a deliberate menu/CLI action.
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void Editor::Open() {
    brush = CreateSolidBrush(PaperColor(view->note.style));
    font = CreateFontW(-static_cast<int>(original.size * view->scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, FontName(view->note.style));
    window = CreateWindowExW(app->testUI ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW, EditorClass, L"Paperdesk / Writing", WS_POPUP, 0, 0, 1, 1, view->window, nullptr, app->instance, this);
    if (!window) throw std::runtime_error("Could not open the text editor");
    input = CreateWindowExW(0, L"EDIT", accepted.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL, 0, 0, 1, 1, window, reinterpret_cast<HMENU>(101), app->instance, nullptr);
    if (!input) throw std::runtime_error("Could not create the text field");
    SetWindowLongPtrW(input, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    inputProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(input, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(InputProc)));
    SendMessageW(input, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(input, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    SendMessageW(input, EM_SETLIMITTEXT, MaxText, 0);
    Position();
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window); SetFocus(input);
    SendMessageW(input, EM_SETSEL, static_cast<WPARAM>(accepted.size()), static_cast<LPARAM>(accepted.size()));
}
void Editor::Position() {
    if (!input) return;
    const auto& b = view->note.blocks[index];
    int width = static_cast<int>((view->note.width - Margin - b.x) * view->scale);
    int maxHeight = static_cast<int>((view->note.height - Margin - b.y) * view->scale);
    int x = view->note.x + static_cast<int>((Shadow + b.x) * view->scale);
    int y = view->note.y + static_cast<int>((Shadow + b.y) * view->scale);
    // Give EDIT its final width before reading its own wrapping metrics.
    SetWindowPos(window, HWND_TOP, x, y, width, maxHeight, SWP_NOACTIVATE | SWP_NOREDRAW);
    MoveWindow(input, 0, 0, width, maxHeight, FALSE);
    RECT format{0, 0, width, maxHeight};
    SendMessageW(input, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&format));
    HDC dc = GetDC(input); auto old = SelectObject(dc, font);
    TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
    SelectObject(dc, old); ReleaseDC(input, dc);
    int lines = static_cast<int>(SendMessageW(input, EM_GETLINECOUNT, 0, 0));
    int height = std::min(maxHeight, std::max(1, lines) * static_cast<int>(metrics.tmHeight) + 3);
    SetWindowPos(window, HWND_TOP, x, y, width, height, SWP_NOACTIVATE);
    MoveWindow(input, 0, 0, width, height, TRUE);
    format = {0, 0, width, height};
    SendMessageW(input, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&format));
    auto first = SendMessageW(input, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (first) SendMessageW(input, EM_LINESCROLL, 0, -first);
}
void Editor::Change() {
    if (changing || ending || !input) return;
    int count = GetWindowTextLengthW(input);
    std::wstring text(static_cast<size_t>(count) + 1, L'\0');
    GetWindowTextW(input, text.data(), count + 1); text.resize(count);
    auto candidate = view->note.blocks[index]; candidate.text = text;
    HDC dc = GetDC(input); auto old = SelectObject(dc, font);
    RECT area{}; GetClientRect(input, &area);
    TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
    int nativeHeight = static_cast<int>(SendMessageW(input, EM_GETLINECOUNT, 0, 0)) * metrics.tmHeight;
    SelectObject(dc, old); ReleaseDC(input, dc);
    int maxHeight = static_cast<int>((view->note.height - Margin - candidate.y) * view->scale);
    if (!Fits(view->note, candidate) || nativeHeight > maxHeight || text.size() > MaxText) {
        DWORD start = 0, end = 0; SendMessageW(input, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        changing = true;
        // Native undo restores the exact selection and keeps accepted text intact on overflow.
        if (SendMessageW(input, EM_CANUNDO, 0, 0)) SendMessageW(input, EM_UNDO, 0, 0);
        if (GetWindowTextLengthW(input) != static_cast<int>(accepted.size())) SetWindowTextW(input, accepted.c_str());
        std::wstring restored(accepted.size() + 1, L'\0');
        GetWindowTextW(input, restored.data(), static_cast<int>(restored.size())); restored.resize(accepted.size());
        if (restored != accepted) SetWindowTextW(input, accepted.c_str());
        SendMessageW(input, EM_SETSEL, std::min<size_t>(start, accepted.size()), std::min<size_t>(end, accepted.size()));
        changing = false;
        Position();
        view->full = true; SetTimer(view->window, FullTimer, 2200, nullptr); view->Paint();
        MessageBeep(MB_OK);
        return;
    }
    accepted = text;
    view->note.blocks[index].text = text;
    Position();
    app->Dirty();
}
void Editor::Finish(bool cancel) {
    ending = true;
    if (cancel) view->note.blocks[index] = original;
    if ((cancel && added) || view->note.blocks[index].text.empty()) view->note.blocks.erase(view->note.blocks.begin() + index);
    DestroyWindow(window); window = nullptr; input = nullptr;
    index = -1; view->full = false; view->Paint();
    app->Dirty(); app->Save();
    app->Layer();
}
LRESULT CALLBACK Editor::InputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* e = reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!e) return DefWindowProcW(hwnd, msg, wp, lp);
    if (msg == WM_SETTEXT) {
        LRESULT result = CallWindowProcW(e->inputProc, hwnd, msg, wp, lp);
        e->Change(); // Multiline EDIT does not emit EN_CHANGE for WM_SETTEXT.
        return result;
    }
    if (msg == WM_KEYDOWN) {
        if (wp == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessageW(hwnd, EM_SETSEL, 0, -1); return 0; }
        if (wp == VK_ESCAPE || (wp == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000))) {
            PostMessageW(e->app->control, FinishEditor, wp == VK_ESCAPE, reinterpret_cast<LPARAM>(e->window)); return 0;
        }
    }
    if (msg == WM_CHAR && (wp == VK_ESCAPE || (wp == L'\n') || (wp == L'\r' && (GetKeyState(VK_CONTROL) & 0x8000)))) return 0;
    if (msg == WM_MOUSEWHEEL) return 0;
    return CallWindowProcW(e->inputProc, hwnd, msg, wp, lp);
}
LRESULT CALLBACK Editor::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* e = reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        e = static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
    }
    if (!e) return DefWindowProcW(hwnd, msg, wp, lp);
    if (msg == WM_COMMAND && HIWORD(wp) == EN_CHANGE) { e->Change(); return 0; }
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
        HDC dc = reinterpret_cast<HDC>(wp); SetTextColor(dc, RGB(60, 52, 43)); SetBkColor(dc, PaperColor(e->view->note.style));
        return reinterpret_cast<LRESULT>(e->brush);
    }
    if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE && !e->ending) PostMessageW(e->app->control, FinishEditor, FALSE, reinterpret_cast<LPARAM>(hwnd));
    if (msg == WM_CLOSE) { PostMessageW(e->app->control, FinishEditor, FALSE, reinterpret_cast<LPARAM>(hwnd)); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
LRESULT CALLBACK App::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* a = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        a = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        a->control = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(a));
    }
    if (!a) return DefWindowProcW(hwnd, msg, wp, lp);
    try {
        switch (msg) {
        case WM_TIMER:
            if (wp == SaveTimer) a->Save();
            if (wp == DesktopTimer) { KillTimer(hwnd, DesktopTimer); a->Layer(); }
            return 0;
        case DesktopEvent:
            a->Layer(); SetTimer(hwnd, DesktopTimer, 120, nullptr); return 0;
        case FinishEditor:
            if (a->editor && a->editor->window == reinterpret_cast<HWND>(lp)) a->EndEdit(wp != 0);
            return 0;
        case WM_HOTKEY: a->New(Style::Butter, L""); return 0;
        case WM_DISPLAYCHANGE:
            a->EndEdit(); for (auto& v : a->notes) v->Place(); a->Dirty(); return 0;
        case WM_COPYDATA: {
            auto* data = reinterpret_cast<COPYDATASTRUCT*>(lp);
            if (data->dwData != IpcMagic || data->cbData < sizeof(wchar_t) || data->cbData > 32768 || data->cbData % sizeof(wchar_t)) return FALSE;
            auto* text = static_cast<const wchar_t*>(data->lpData);
            if (!text || text[data->cbData / sizeof(wchar_t) - 1] != L'\0') return FALSE;
            a->commands.push_back(Parse(text));
            PostMessageW(hwnd, ExecuteCommand, 0, 0);
            return TRUE;
        }
        case ExecuteCommand:
            if (!a->commands.empty()) {
                auto command = std::move(a->commands.front()); a->commands.pop_front();
                a->RunCommand(command);
            }
            return 0;
        case WM_QUERYENDSESSION: a->EndEdit(); return a->Save() ? TRUE : FALSE;
        case WM_ENDSESSION: if (wp) { a->EndEdit(); a->Save(); } return 0;
        case WM_CLOSE:
            a->EndEdit();
            if (!a->Save()) return 0;
            a->shuttingDown = true;
            for (auto& v : a->notes) DestroyWindow(v->window);
            UnregisterHotKey(hwnd, 1); DestroyWindow(hwnd); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
    } catch (const std::exception& e) { Error(hwnd, e); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void Register(HINSTANCE instance, const wchar_t* name, WNDPROC proc, UINT style = 0) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance; wc.lpszClassName = name; wc.lpfnWndProc = proc; wc.style = style;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) throw std::runtime_error("Could not register a window class");
}
std::wstring ProfileKey(const std::filesystem::path& directory) {
    auto canonical = std::filesystem::weakly_canonical(directory).wstring();
    CharLowerBuffW(canonical.data(), static_cast<DWORD>(canonical.size()));
    uint64_t hash = 14695981039346656037ull;
    for (wchar_t c : canonical) { hash ^= c; hash *= 1099511628211ull; }
    return std::to_wstring(hash);
}
int Run() {
    auto o = Parse(GetCommandLineW());
    if (o.help) { Help(nullptr); return 0; }
    if (!o.startup.empty()) { Startup(o.startup == L"enable"); return 0; }
    if (!o.preview.empty()) { RenderPreview(o.preview); return 0; }
    auto key = ProfileKey(o.directory);
    auto controllerName = L"Paperdesk.Controller." + key;
    HANDLE mutex = CreateMutexW(nullptr, FALSE, (L"Local\\Paperdesk." + key).c_str());
    if (!mutex) throw std::runtime_error("Could not acquire the single-instance lock");
    struct Close { HANDLE h; ~Close() { CloseHandle(h); } } close{mutex};
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = nullptr;
        for (int i = 0; i < 40 && !existing; ++i) { existing = FindWindowW(controllerName.c_str(), nullptr); if (!existing) Sleep(50); }
        if (!existing) throw std::runtime_error("Paperdesk is starting. Try the command again.");
        DWORD pid = 0; GetWindowThreadProcessId(existing, &pid); AllowSetForegroundWindow(pid);
        std::wstring command = GetCommandLineW();
        COPYDATASTRUCT data{IpcMagic, static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)), command.data()};
        DWORD_PTR result = 0;
        if (!SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 3000, &result) || !result) throw std::runtime_error("The running Paperdesk did not accept the command");
        return 0;
    }
    if (o.quit) return 0;
    App app(o);
    auto loaded = app.store.Load();
    Register(app.instance, NoteClass, NoteView::Proc, CS_DBLCLKS);
    Register(app.instance, EditorClass, Editor::Proc);
    Register(app.instance, controllerName.c_str(), App::Proc);
    app.control = CreateWindowExW(WS_EX_TOOLWINDOW, controllerName.c_str(), L"Paperdesk", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, app.instance, &app);
    if (!app.control) throw std::runtime_error("Could not create the message window");
    running = &app;
    for (auto& n : loaded.state.notes) app.Add(std::move(n));
    bool firstRun = !std::filesystem::exists(app.store.Directory() / L"notes.paper") && !loaded.recovered;
    if (firstRun && !o.create) {
        Note welcome;
        welcome.id = NewId();
        welcome.width = 330; welcome.height = 340;
        welcome.blocks = {{32, 40, 25, L"Make yourself\nat home."}, {32, 151, 17, L"Double-click to write.\nAlt + drag to move.\nRight-click for more."}, {136, 279, 16, L"a little less noise."}};
        app.Add(std::move(welcome)); app.Dirty(); app.Save();
    }
    if (!o.noHotkey && !RegisterHotKey(app.control, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N')) {
        MessageBoxW(nullptr, L"Ctrl+Alt+N is already in use. Paperdesk still works: launch Paperdesk.exe --new or use a note's right-click menu.", L"Paperdesk", MB_OK | MB_ICONINFORMATION);
    }
    app.foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, Foreground, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!app.foregroundHook) throw std::runtime_error("Could not subscribe to desktop focus changes");
    if (loaded.recovered) MessageBoxW(nullptr, L"Your notes were recovered from the previous autosave. The damaged file, if present, was kept in the data folder.", L"Paperdesk", MB_OK | MB_ICONINFORMATION);
    if (o.create) app.New(o.style, o.text);
    else app.Layer();
    MSG msg{};
    BOOL status;
    while ((status = GetMessageW(&msg, nullptr, 0, 0)) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    running = nullptr;
    return status == -1 ? 1 : 0;
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
    int code = 1;
    try { code = Run(); }
    catch (const std::exception& e) { Error(nullptr, e); }
    Gdiplus::GdiplusShutdown(token);
    if (SUCCEEDED(com)) CoUninitialize();
    return code;
}
