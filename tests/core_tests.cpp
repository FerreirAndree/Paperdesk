#include "model.h"
#include "storage.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
#include <stdexcept>
using namespace paper;
static int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
void Reject(const std::function<void()>& action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, message);
}
int main() {
    auto folder = std::filesystem::temp_directory_path() / (L"Paperdesk-test-" + NewId());
    try {
        State state;
        Note n; n.id = NewId(); n.x = -1440; n.y = -20; n.style = Style::Parchment;
        n.blocks = {{32, 46, 22, L"Quotes: \"hello\" \\ path\r\nUnicode: ação 日本語 \U0001F33F"}, {80, 140, 17, L"Second block"}};
        state.notes.push_back(n);
        auto encoded = Encode(state);
        Check(Encode(Decode(encoded)) == encoded, "Unicode / multiline round trip changed data");
        Check(Decode(encoded).notes[0].x == -1440, "Negative monitor position was lost");
        Check(Decode("PAPERDESK 1\n0\n").notes.empty(), "Empty state failed");
        Check(WindowsNewlines(L"a\nb\r\nc\rd") == L"a\r\nb\r\nc\r\nd", "Native editor newline conversion failed");
        Check(WindowsNewlines(WindowsNewlines(L"a\nb")) == L"a\r\nb", "Newline conversion was not idempotent");
        Reject([&] { Decode("PAPERDESK 2\n0\n"); }, "Future format was accepted");
        Reject([&] { Decode("PAPERDESK 1\n65\n"); }, "Note cap was bypassed");
        Reject([&] { Decode(encoded + "junk"); }, "Trailing corruption was accepted");
        Reject([&] { Wide(std::string("\xC0\xAF", 2)); }, "Invalid UTF-8 was accepted");
        for (size_t i = 0; i + 2 < encoded.size(); ++i) {
            Reject([&] { Decode(encoded.substr(0, i)); }, "A truncated record was accepted");
        }
        auto bad = state; bad.notes.push_back(n);
        Reject([&] { Encode(bad); }, "Duplicate note IDs were accepted");
        bad = state; bad.notes[0].width = 0;
        Reject([&] { Encode(bad); }, "Zero-size note was accepted");
        bad = state; bad.notes[0].blocks[0].text = std::wstring(MaxText + 1, L'x');
        Reject([&] { Encode(bad); }, "Text limit was bypassed");
        bad = state; bad.notes[0].blocks[0].x = 9999;
        Reject([&] { Encode(bad); }, "Off-paper block was accepted");
        Store store(folder);
        Check(store.Load().state.notes.empty(), "A fresh directory was not empty");
        store.Save(state);
        Check(Encode(store.Load().state) == encoded, "Disk round trip changed data");
        auto second = state; second.notes[0].blocks[0].text = L"Second save";
        store.Save(second);
        Check(Encode(store.Load().state) == Encode(second), "Replacement save failed");
        Check(std::filesystem::exists(folder / L"notes.backup.paper"), "Atomic replacement did not create backup");
        { std::ofstream f(folder / L"notes.paper", std::ios::binary); f << "broken"; }
        auto recovered = store.Load();
        Check(recovered.recovered, "Corruption did not trigger recovery");
        Check(Encode(recovered.state) == encoded, "Recovery did not use previous good save");
        store.Save(recovered.state);
        Check(!store.Load().recovered, "Recovery could not be committed");
        // A newer uncommitted temporary file must never override the committed state.
        { std::ofstream f(folder / L"notes.pending.paper", std::ios::binary); f << Encode(second); }
        Check(Encode(store.Load().state) == encoded, "Uncommitted data overrode committed notes");
        store.Save(State{});
        Check(store.Load().state.notes.empty(), "Deleting the final note did not persist");
        { std::ofstream f(folder / L"notes.paper"); f << "broken"; }
        { std::ofstream f(folder / L"notes.backup.paper"); f << "also broken"; }
        Reject([&] { store.Load(); }, "Two damaged saves silently created an empty desktop");
        Check(std::filesystem::exists(folder / L"notes.paper"), "Failed recovery destroyed original data");
        std::filesystem::remove_all(folder); // Unique test-owned directory created above.
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\nTest data: " << folder << '\n';
        return 1;
    }
}
