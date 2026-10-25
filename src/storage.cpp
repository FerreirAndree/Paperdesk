#include "storage.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <stdexcept>

namespace paper {
std::filesystem::path DefaultDirectory() {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) throw std::runtime_error("Could not locate LocalAppData");
    std::filesystem::path result = std::filesystem::path(path) / L"Paperdesk";
    CoTaskMemFree(path);
    return result;
}
Store::Store(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::filesystem::create_directories(directory_);
}
static State Read(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 16 * 1024 * 1024) throw std::runtime_error("Note file is too large");
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open note file");
    std::string data{std::istreambuf_iterator<char>(file), {}};
    if (file.bad()) throw std::runtime_error("Could not read note file");
    return Decode(data);
}
LoadResult Store::Load() const {
    const auto primary = directory_ / L"notes.paper";
    const auto backup = directory_ / L"notes.backup.paper";
    if (!std::filesystem::exists(primary)) {
        if (std::filesystem::exists(backup)) return {Read(backup), true};
        return {};
    }
    try { return {Read(primary), false}; }
    catch (...) {
        if (!std::filesystem::exists(backup)) throw;
        auto recovered = Read(backup); // Never replace both unreadable files with an empty desktop.
        // Preserve the damaged original for manual recovery before the next save.
        auto damaged = directory_ / (L"notes.damaged." + std::to_wstring(GetTickCount64()) + L".paper");
        std::filesystem::rename(primary, damaged);
        return {std::move(recovered), true};
    }
}
void Store::Save(const State& state) const {
    const auto bytes = Encode(state);
    const auto temporary = directory_ / L"notes.pending.paper";
    const auto primary = directory_ / L"notes.paper";
    const auto backup = directory_ / L"notes.backup.paper";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Could not open the autosave file");
    DWORD written = 0;
    bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!ok) throw std::runtime_error("Could not write notes to disk");
    if (std::filesystem::exists(primary)) {
        if (!ReplaceFileW(primary.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr)) throw std::runtime_error("Could not commit the autosave file");
    } else if (!MoveFileExW(temporary.c_str(), primary.c_str(), MOVEFILE_WRITE_THROUGH)) {
        throw std::runtime_error("Could not commit the first autosave file");
    }
}
}
