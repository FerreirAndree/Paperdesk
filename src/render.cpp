#include "render.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace Gdiplus;

namespace paper {
const wchar_t* FontName(Style style) { return style == Style::Parchment ? L"Georgia" : L"Segoe Print"; }
COLORREF PaperColor(Style style) {
    if (style == Style::Rose) return RGB(238, 191, 185);
    if (style == Style::Parchment) return RGB(241, 226, 194);
    return RGB(244, 221, 129);
}
static StringFormat TextFormat() {
    return StringFormat(StringFormatFlagsMeasureTrailingSpaces);
}
RectF BlockBounds(const Note& note, const Block& block) {
    Bitmap bitmap(1, 1, PixelFormat32bppPARGB);
    Graphics g(&bitmap);
    g.SetPageUnit(UnitPixel);
    Font font(FontName(note.style), block.size, FontStyleRegular, UnitPixel);
    auto format = TextFormat();
    RectF bounds;
    RectF layout(0, 0, std::max(1.f, note.width - Margin - block.x), 100000);
    std::wstring text = block.text.empty() ? L" " : block.text;
    // A trailing newline needs room for the next line, too.
    if (!text.empty() && (text.back() == L'\n' || text.back() == L'\r')) text += L" ";
    g.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font, layout, &format, &bounds);
    return RectF(block.x, block.y, std::max(bounds.Width, 12.f), std::max(bounds.Height, block.size * 1.3f));
}
bool Fits(const Note& note, const Block& block) {
    if (block.x < Margin || block.y < Margin || block.x > note.width - Margin - 16 || block.y > note.height - Margin - 16 || block.text.size() > MaxText) return false;
    auto bounds = BlockBounds(note, block);
    return bounds.GetRight() <= note.width - Margin + 0.5f && bounds.GetBottom() <= note.height - Margin + 0.5f;
}
bool AllFit(const Note& note) {
    return std::all_of(note.blocks.begin(), note.blocks.end(), [&](const auto& b) { return Fits(note, b); });
}
static void PaperPath(GraphicsPath& path, float w, float h) {
    path.AddLine(1.f, 1.f, w - 1, 1.f);
    path.AddLine(w - 1, 1.f, w - 2, h - 12);
    path.AddBezier(w - 2, h - 12, w - 30, h - 3, w * .5f, h, 1.f, h - 6);
    path.CloseFigure();
}
static void DrawParchment(Graphics& g, float width, float height) {
    // Cache decoded pixels, not GDI+ objects: the cache remains safe after shutdown.
    struct Texture { UINT width = 0, height = 0; std::vector<BYTE> pixels; };
    static const Texture texture = [] {
        HMODULE module = GetModuleHandleW(nullptr);
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(102), RT_RCDATA);
        DWORD size = resource ? SizeofResource(module, resource) : 0;
        HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
        const void* source = loaded ? LockResource(loaded) : nullptr;
        if (!source || !size) throw std::runtime_error("The parchment texture is missing");
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory) throw std::runtime_error("Could not allocate the parchment texture");
        void* bytes = GlobalLock(memory);
        if (!bytes) { GlobalFree(memory); throw std::runtime_error("Could not read the parchment texture"); }
        memcpy(bytes, source, size); GlobalUnlock(memory);
        IStream* raw = nullptr;
        if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &raw))) {
            GlobalFree(memory); throw std::runtime_error("Could not open the parchment texture");
        }
        auto release = [](IStream* stream) { stream->Release(); };
        std::unique_ptr<IStream, decltype(release)> stream(raw, release);
        Bitmap decoded(stream.get());
        if (decoded.GetLastStatus() != Ok) throw std::runtime_error("Could not decode the parchment texture");
        Texture result; result.width = decoded.GetWidth(); result.height = decoded.GetHeight();
        result.pixels.resize(static_cast<size_t>(result.width) * result.height * 4);
        BitmapData data{}; Rect rect(0, 0, result.width, result.height);
        if (decoded.LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &data) != Ok)
            throw std::runtime_error("Could not access the parchment pixels");
        for (UINT y = 0; y < result.height; ++y)
            memcpy(result.pixels.data() + static_cast<size_t>(y) * result.width * 4,
                static_cast<const BYTE*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride, result.width * 4);
        decoded.UnlockBits(&data);
        return result;
    }();
    Bitmap paper(texture.width, texture.height, texture.width * 4, PixelFormat32bppPARGB,
        const_cast<BYTE*>(texture.pixels.data()));
    auto state = g.Save();
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    ImageAttributes attributes; attributes.SetWrapMode(WrapModeTileFlipXY);
    // Make the near-opaque paper body solid while retaining soft torn-edge alpha.
    ColorMatrix opacity{{{1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1.02f, 0}, {0, 0, 0, 0, 1}}};
    attributes.SetColorMatrix(&opacity);
    g.DrawImage(&paper, RectF(0, 0, width, height), 0, 0,
        static_cast<float>(texture.width), static_cast<float>(texture.height), UnitPixel, &attributes);
    g.Restore(state);
}
std::unique_ptr<Bitmap> DrawNote(const Note& note, float scale, int hiddenBlock, bool full) {
    int width = static_cast<int>(std::ceil((note.width + Shadow * 2) * scale));
    int height = static_cast<int>(std::ceil((note.height + Shadow * 2) * scale));
    auto bitmap = std::make_unique<Bitmap>(width, height, PixelFormat32bppPARGB);
    Graphics g(bitmap.get());
    g.SetPageUnit(UnitPixel);
    g.Clear(Color(0, 0, 0, 0));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(scale, scale);
    g.TranslateTransform(Shadow, Shadow);
    const float w = note.width, h = note.height;
    auto clipped = g.Save();
    if (note.style == Style::Parchment) {
        DrawParchment(g, w, h);
        g.SetClip(RectF(0, 0, w, h));
    } else {
        GraphicsPath path;
        PaperPath(path, w, h);
        // Small contact shadow with a softer lift along the free lower edge.
        for (int i = 10; i >= 1; --i) {
            auto saved = g.Save();
            g.TranslateTransform(0, 2 + i * .4f);
            Pen shadow(Color(static_cast<BYTE>(i == 1 ? 27 : 4), 26, 22, 15), static_cast<float>(i));
            g.DrawPath(&shadow, &path);
            g.Restore(saved);
        }
        auto base = PaperColor(note.style);
        Color top(255, static_cast<BYTE>(std::min(255, GetRValue(base) + 8)), static_cast<BYTE>(std::min(255, GetGValue(base) + 9)), static_cast<BYTE>(std::min(255, GetBValue(base) + 14)));
        Color bottom(255, GetRValue(base), GetGValue(base), GetBValue(base));
        LinearGradientBrush paper(PointF(0, 0), PointF(w * .28f, h), top, bottom);
        paper.SetWrapMode(WrapModeTileFlipXY);
        g.FillPath(&paper, &path);
        g.SetClip(&path);
        LinearGradientBrush glue(PointF(0, 0), PointF(0, 43), Color(16, 117, 94, 49), Color(0, 117, 94, 49));
        glue.SetWrapMode(WrapModeTileFlipXY);
        g.FillRectangle(&glue, 0.f, 0.f, w, 43.f);
        LinearGradientBrush curl(PointF(0, h - 24), PointF(0, h), Color(0, 255, 255, 232), Color(42, 255, 255, 232));
        curl.SetWrapMode(WrapModeTileFlipXY);
        g.FillRectangle(&curl, 0.f, h - 24, w, 24.f);
        // Deterministic fibres, painted only on changes. No images, timers or animation loop.
        uint32_t seed = 3721;
        auto random = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.f; };
        int grains = static_cast<int>(w * h / 29);
        for (int i = 0; i < grains; ++i) {
            float x = random() * w, y = random() * h;
            SolidBrush grain(Color(static_cast<BYTE>(5 + random() * 9), 115, 86, 43));
            g.FillRectangle(&grain, x, y, .45f + random() * .7f, .45f);
        }
        Pen rim(Color(48, 140, 110, 63), .65f);
        g.DrawPath(&rim, &path);
    }
    SolidBrush ink(Color(245, 60, 52, 43));
    auto format = TextFormat();
    for (size_t i = 0; i < note.blocks.size(); ++i) {
        if (static_cast<int>(i) == hiddenBlock) continue;
        const auto& b = note.blocks[i];
        Font font(FontName(note.style), b.size, FontStyleRegular, UnitPixel);
        RectF bounds(b.x, b.y, std::max(1.f, w - Margin - b.x), std::max(1.f, h - Margin - b.y));
        g.DrawString(b.text.c_str(), static_cast<INT>(b.text.size()), &font, bounds, &format, &ink);
    }
    if (full) {
        Font font(L"Segoe UI", 11, FontStyleRegular, UnitPixel);
        SolidBrush warn(Color(255, 126, 63, 49));
        g.DrawString(L"Paper is full. Use smaller text.", -1, &font, PointF(20, h - 20), &warn);
    }
    g.Restore(clipped);
    return bitmap;
}
bool Present(HWND window, const Note& note, float scale, int hiddenBlock, bool full) {
    auto bitmap = DrawNote(note, scale, hiddenBlock, full);
    BitmapData data{};
    Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
    if (bitmap->LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &data) != Ok) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.Width;
    info.bmiHeader.biHeight = -rect.Height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(nullptr), memory = CreateCompatibleDC(screen);
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (dib && bits && memory) {
        for (int y = 0; y < rect.Height; ++y) memcpy(static_cast<BYTE*>(bits) + y * rect.Width * 4, static_cast<BYTE*>(data.Scan0) + y * data.Stride, rect.Width * 4);
        auto old = SelectObject(memory, dib);
        POINT position{note.x, note.y}, source{};
        SIZE size{rect.Width, rect.Height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        ok = UpdateLayeredWindow(window, screen, &position, &size, memory, &source, 0, &blend, ULW_ALPHA) != FALSE;
        SelectObject(memory, old);
    }
    if (dib) DeleteObject(dib);
    if (memory) DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    bitmap->UnlockBits(&data);
    return ok;
}
void RenderPreview(const std::wstring& path) {
    Bitmap output(1260, 570, PixelFormat32bppARGB);
    Graphics g(&output);
    g.Clear(Color(255, 44, 55, 52));
    for (int i = 0; i < 3; ++i) {
        Note n;
        n.style = static_cast<Style>(i);
        n.width = 310; n.height = 320;
        if (i == 0) n.blocks = {{32, 40, 27, L"Before Friday"}, {33, 108, 18, L"Send the final files.\nBook the train.\nBack up the project."}, {129, 258, 16, L"16:30 / review"}};
        if (i == 1) n.blocks = {{32, 42, 26, L"Next build"}, {33, 109, 18, L"Keyboard navigation\nOffline mode\nExport to PDF"}, {123, 258, 16, L"test on laptop"}};
        if (i == 2) n.blocks = {{32, 43, 28, L"Field notes"}, {33, 113, 21, L"North trail, 8 km.\nWater at the old mill.\nLast train: 18:40."}, {174, 258, 16, L"26 October"}};
        if (!AllFit(n)) throw std::runtime_error("Preview text does not fit the note");
        auto image = DrawNote(n, 1.1f);
        g.DrawImage(image.get(), 55 + i * 397, 91);
    }
    UINT count = 0, size = 0;
    GetImageEncodersSize(&count, &size);
    std::vector<BYTE> buffer(size);
    auto encoders = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    GetImageEncoders(count, size, encoders);
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
            if (output.Save(path.c_str(), &encoders[i].Clsid) != Ok) throw std::runtime_error("Could not save preview");
            return;
        }
    }
    throw std::runtime_error("PNG encoder unavailable");
}
}
