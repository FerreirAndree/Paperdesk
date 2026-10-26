#include "render.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
using namespace paper;
static int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
int InkLines(Gdiplus::Bitmap& image, const Note& note, float scale) {
    int lines = 0, empty = 1000;
    // Close small gaps within glyphs, including separated dots at low zoom.
    int gap = std::max(4, static_cast<int>(10 * scale));
    for (int y = static_cast<int>((Shadow + Margin) * scale); y < static_cast<int>((Shadow + note.height - Margin) * scale); ++y) {
        int ink = 0;
        for (int x = static_cast<int>((Shadow + Margin) * scale); x < static_cast<int>((Shadow + note.width - Margin) * scale); ++x) {
            Gdiplus::Color p; image.GetPixel(x, y, &p);
            if (p.GetR() < 140 && p.GetG() < 130 && p.GetB() < 110) ++ink;
        }
        if (ink > 2) { if (empty >= gap) ++lines; empty = 0; }
        else ++empty;
    }
    return lines;
}
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
        note.blocks = {{32, 40, 25, L"Your desktop.\nYour notes."}, {32, 151, 17, L"Double-click to write.\nAlt + drag to move.\nRight-click for commands."}, {180, 279, 16, L"quick start"}};
        Check(AllFit(note), "The welcome note clips its own instructions");
        note.height = MinHeight;
        Check(!AllFit(note), "Shrinking accepted clipped content");
        Note reminder;
        reminder.blocks = {{32, 43, 22, L"A note that scales without changing its layout."}};
        for (float zoom : {.35f, .5f, .72f, 1.f, 1.5f, 2.f}) {
            reminder.zoom = zoom;
            auto image = DrawNote(reminder, zoom);
            Check(image->GetWidth() == static_cast<UINT>(std::ceil((reminder.width + 2 * Shadow) * zoom)), "Scaled image dimensions are incorrect");
            auto lines = InkLines(*image, reminder, zoom);
            std::cout << "zoom=" << zoom << " ink lines=" << lines << '\n';
            if (lines != 3) {
                const CLSID png{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
                image->Save(L"render-layout-failure.png", &png);
            }
            Check(lines == 3, "Scaling changed the rendered line breaks");
        }
        std::cout << checks << " rendering and layout checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    Gdiplus::GdiplusShutdown(token);
    return result;
}
