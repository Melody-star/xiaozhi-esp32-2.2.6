#include "graphics.h"
#include "frame_buffer.h"
#include "matrix_mapper.h"
#include "strip_driver.h"
#include "font.h"
#include <algorithm>
#include <cstdlib>

Graphics::Graphics(FrameBuffer& fb, MatrixMapper& mapper, LedStripDriver& driver, Font& font)
    : fb_(fb), mapper_(mapper), driver_(driver), font_(font) {}

void Graphics::DrawPixelClipped(int16_t x, int16_t y, const Color& color) {
    if (x < 0 || x >= (int16_t)fb_.Width() || y < 0 || y >= (int16_t)fb_.Height()) return;
    fb_.SetPixel((uint16_t)x, (uint16_t)y, color);
}

void Graphics::DrawPixel(int16_t x, int16_t y, const Color& color) {
    DrawPixelClipped(x, y, color);
}

void Graphics::DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const Color& color) {
    int16_t dx = abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    while (true) {
        DrawPixelClipped(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Graphics::DrawRect(int16_t x, int16_t y, uint16_t w, uint16_t h, const Color& color) {
    for (uint16_t i = 0; i < w; i++) {
        DrawPixelClipped(x + i, y, color);
        DrawPixelClipped(x + i, y + h - 1, color);
    }
    for (uint16_t i = 1; i < h - 1; i++) {
        DrawPixelClipped(x, y + i, color);
        DrawPixelClipped(x + w - 1, y + i, color);
    }
}

void Graphics::FillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, const Color& color) {
    int16_t x0 = std::max<int16_t>(x, 0);
    int16_t y0 = std::max<int16_t>(y, 0);
    int16_t x1 = std::min<int16_t>(x + w, fb_.Width());
    int16_t y1 = std::min<int16_t>(y + h, fb_.Height());
    for (int16_t j = y0; j < y1; j++) {
        for (int16_t i = x0; i < x1; i++) {
            fb_.SetPixel((uint16_t)i, (uint16_t)j, color);
        }
    }
}

void Graphics::DrawCircle(int16_t cx, int16_t cy, int16_t r, const Color& color) {
    int16_t x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        auto plot = [&](int16_t ox, int16_t oy) {
            DrawPixelClipped(cx + ox, cy + oy, color);
            DrawPixelClipped(cx - ox, cy + oy, color);
            DrawPixelClipped(cx + ox, cy - oy, color);
            DrawPixelClipped(cx - ox, cy - oy, color);
        };
        plot(x, y);
        plot(y, x);
        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void Graphics::FillCircle(int16_t cx, int16_t cy, int16_t r, const Color& color) {
    for (int16_t y = cy - r; y <= cy + r; y++) {
        for (int16_t x = cx - r; x <= cx + r; x++) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) {
                DrawPixelClipped(x, y, color);
            }
        }
    }
}

void Graphics::DrawBitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t* data, const Color& color) {
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            uint16_t idx = j * w + i;
            if (data[idx / 8] & (0x80 >> (idx % 8))) {
                DrawPixelClipped(x + i, y + j, color);
            }
        }
    }
}

void Graphics::DrawChar(int16_t x, int16_t y, char c, const Color& color) {
    if (!font_.HasChar(c)) return;
    const Glyph* g = font_.GetGlyph(c);
    if (!g) return;
    const uint8_t* bitmap = font_.GetBitmap(c);
    if (!bitmap) return;

    for (uint8_t col = 0; col < g->width; col++) {
        uint8_t bits = bitmap[col];
        for (uint8_t row = 0; row < g->height; row++) {
            if (bits & (1 << row)) {
                DrawPixelClipped(x + col, y + row, color);
            }
        }
    }
}

void Graphics::DrawString(int16_t x, int16_t y, const std::string& str, const Color& color) {
    int16_t cursor = x;
    for (char c : str) {
        DrawChar(cursor, y, c, color);
        cursor += font_.CharAdvance(c);
    }
}

void Graphics::Clear(const Color& color) {
    fb_.Clear(color);
}

void Graphics::Refresh() {
    for (uint16_t y = 0; y < fb_.Height(); y++) {
        for (uint16_t x = 0; x < fb_.Width(); x++) {
            uint16_t idx = mapper_.XY(x, y);
            if (idx == 0xFFFF) continue;
            driver_.SetPixel(idx, fb_.GetPixel(x, y));
        }
    }
    driver_.Refresh();
}
