#include "model.h"
#include <windows.h>
#include <objbase.h>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace paper {
std::string Utf8(std::wstring_view text) {
    if (text.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("Invalid Unicode text");
    std::string out(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), count, nullptr, nullptr);
    return out;
}
std::wstring Wide(std::string_view text) {
    if (text.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!count) throw std::runtime_error("Invalid UTF-8 data");
    std::wstring out(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), count);
    return out;
}
std::wstring NewId() {
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Could not create a note identifier");
    wchar_t buffer[40]{};
    StringFromGUID2(guid, buffer, 40);
    return buffer;
}
std::wstring WindowsNewlines(std::wstring_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            result += L"\r\n";
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        } else if (text[i] == L'\n') result += L"\r\n";
        else result += text[i];
    }
    return result;
}
const wchar_t* StyleName(Style style) {
    switch (style) {
    case Style::Butter: return L"Butter";
    case Style::Rose: return L"Rose";
    case Style::Parchment: return L"Parchment";
    }
    return L"Butter";
}
Style ParseStyle(std::wstring_view text) {
    if (text == L"butter") return Style::Butter;
    if (text == L"rose") return Style::Rose;
    if (text == L"parchment") return Style::Parchment;
    throw std::runtime_error("Style must be butter, rose, or parchment");
}
void Validate(const State& state) {
    if (state.notes.size() > MaxNotes) throw std::runtime_error("Too many notes");
    std::set<std::wstring> ids;
    for (const auto& n : state.notes) {
        if (n.id.empty() || n.id.size() > 80 || !ids.insert(n.id).second) throw std::runtime_error("Invalid or duplicate note identifier");
        if (n.x < -100000 || n.x > 100000 || n.y < -100000 || n.y > 100000) throw std::runtime_error("Invalid note position");
        if (!std::isfinite(n.width) || !std::isfinite(n.height) || n.width < MinWidth || n.height < MinHeight || n.width > MaxSize || n.height > MaxSize) throw std::runtime_error("Invalid paper dimensions");
        if (n.style < Style::Butter || n.style > Style::Parchment || n.blocks.size() > MaxBlocks) throw std::runtime_error("Invalid note style or block count");
        for (const auto& b : n.blocks) {
            if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.size) || b.x < Margin || b.y < Margin || b.x > n.width - Margin - 16 || b.y > n.height - Margin - 16 || b.size < 14 || b.size > 40 || b.text.size() > MaxText || b.text.find(L'\0') != std::wstring::npos) throw std::runtime_error("Invalid text block");
        }
    }
}
std::string Encode(const State& state) {
    Validate(state);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "PAPERDESK 1\n" << state.notes.size() << '\n' << std::setprecision(9);
    for (const auto& n : state.notes) {
        out << std::quoted(Utf8(n.id)) << ' ' << n.x << ' ' << n.y << ' ' << n.width << ' ' << n.height << ' ' << static_cast<int>(n.style) << ' ' << n.blocks.size() << '\n';
        for (const auto& b : n.blocks) out << b.x << ' ' << b.y << ' ' << b.size << ' ' << std::quoted(Utf8(b.text)) << '\n';
    }
    return out.str();
}
State Decode(std::string_view bytes) {
    if (bytes.size() > 16 * 1024 * 1024) throw std::runtime_error("Note file exceeds the size limit");
    std::istringstream in{std::string(bytes)};
    in.imbue(std::locale::classic());
    std::string magic;
    int version = 0;
    size_t count = 0;
    if (!(in >> magic >> version >> count) || magic != "PAPERDESK" || version != 1 || count > MaxNotes) throw std::runtime_error("Unrecognised or damaged note file");
    State state;
    for (size_t i = 0; i < count; ++i) {
        Note n;
        std::string id;
        size_t blocks = 0;
        int style = 0;
        if (!(in >> std::quoted(id) >> n.x >> n.y >> n.width >> n.height >> style >> blocks) || blocks > MaxBlocks) throw std::runtime_error("Damaged note record");
        n.id = Wide(id);
        n.style = static_cast<Style>(style);
        for (size_t j = 0; j < blocks; ++j) {
            Block b;
            std::string text;
            if (!(in >> b.x >> b.y >> b.size >> std::quoted(text))) throw std::runtime_error("Damaged text block");
            b.text = Wide(text);
            n.blocks.push_back(std::move(b));
        }
        state.notes.push_back(std::move(n));
    }
    in >> std::ws;
    if (!in.eof()) throw std::runtime_error("Unexpected data after notes");
    Validate(state);
    return state;
}
}
