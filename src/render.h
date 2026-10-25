#pragma once
#include "model.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>

namespace paper {
const wchar_t* FontName(Style style);
COLORREF PaperColor(Style style);
Gdiplus::RectF BlockBounds(const Note& note, const Block& block);
bool Fits(const Note& note, const Block& block);
bool AllFit(const Note& note);
std::unique_ptr<Gdiplus::Bitmap> DrawNote(const Note& note, float scale, int hiddenBlock = -1, bool full = false);
bool Present(HWND window, const Note& note, float scale, int hiddenBlock = -1, bool full = false);
void RenderPreview(const std::wstring& path);
}
