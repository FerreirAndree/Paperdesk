#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace paper {
constexpr size_t MaxNotes = 64;
constexpr size_t MaxBlocks = 32;
constexpr size_t MaxText = 4096;
constexpr float MinWidth = 220, MinHeight = 200, MaxSize = 900;
constexpr float Margin = 28, Shadow = 16;
enum class Style : int { Butter, Rose, Parchment };
struct Block {
    float x = 32, y = 46, size = 22;
    std::wstring text;
};
struct Note {
    std::wstring id;
    int x = 120, y = 120; // Physical desktop coordinates, including negative monitors.
    float width = 310, height = 310; // Paper dimensions in device independent pixels.
    Style style = Style::Butter;
    std::vector<Block> blocks;
};
struct State { std::vector<Note> notes; };
std::string Utf8(std::wstring_view text);
std::wstring Wide(std::string_view text);
std::wstring WindowsNewlines(std::wstring_view text);
std::wstring NewId();
std::string Encode(const State& state);
State Decode(std::string_view bytes);
void Validate(const State& state);
const wchar_t* StyleName(Style style);
Style ParseStyle(std::wstring_view text);
}

