#include "render.h"
#include <iostream>
#include <stdexcept>
using namespace paper;
static int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
int main() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
    int result = 0;
    try {
        Note note;
        for (auto style : {Style::Butter, Style::Rose, Style::Parchment}) {
            note.style = style;
            Block block{32, 43, 22, L"A small reminder.\r\nOne more line."};
            Check(Fits(note, block), "Ordinary multiline content did not fit");
            block.text = std::wstring(4096, L'W');
            Check(!Fits(note, block), "Overflowing text was accepted");
            block.text = L"Bottom edge\r\nSecond line"; block.y = note.height - Margin - 25;
            Check(!Fits(note, block), "Text ran off the bottom edge");
            block.text = L"Bottom edge\r\n";
            Check(!Fits(note, block), "A trailing newline bypassed the space check");
            note.blocks = {{32, 43, 22, L"A small reminder."}};
            for (float scale : {1.f, 1.25f, 1.5f, 2.f}) {
                auto bitmap = DrawNote(note, scale);
                Check(bitmap && bitmap->GetLastStatus() == Gdiplus::Ok, "Paper rendering failed");
                Gdiplus::Color pixel;
                bitmap->GetPixel(0, 0, &pixel);
                Check(pixel.GetA() == 0, "Transparent edge became opaque");
                bitmap->GetPixel(static_cast<INT>(100 * scale), static_cast<INT>(180 * scale), &pixel);
                Check(pixel.GetA() == 255, "Paper body was not opaque");
            }
        }
        note.width = 330; note.height = 340; note.style = Style::Butter;
        note.blocks = {{32, 40, 25, L"Make yourself\nat home."}, {32, 151, 17, L"Double-click to write.\nAlt + drag to move.\nRight-click for more."}, {136, 279, 16, L"a little less noise."}};
        Check(AllFit(note), "The welcome note clips its own instructions");
        note.height = MinHeight;
        Check(!AllFit(note), "Shrinking accepted clipped content");
        std::cout << checks << " rendering and layout checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    Gdiplus::GdiplusShutdown(token);
    return result;
}
