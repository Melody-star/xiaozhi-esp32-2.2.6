#pragma once
#include "color.h"
#include <cstdint>
#include <string>

class FrameBuffer;
class MatrixMapper;
class LedStripDriver;
class Font;

class Graphics {
public:
    Graphics(FrameBuffer& fb, MatrixMapper& mapper, LedStripDriver& driver, Font& font);

    void DrawPixel(int16_t x, int16_t y, const Color& color);
    void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const Color& color);
    void DrawRect(int16_t x, int16_t y, uint16_t w, uint16_t h, const Color& color);
    void FillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, const Color& color);
    void DrawCircle(int16_t cx, int16_t cy, int16_t r, const Color& color);
    void FillCircle(int16_t cx, int16_t cy, int16_t r, const Color& color);
    void DrawBitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t* data, const Color& color);
    void DrawChar(int16_t x, int16_t y, char c, const Color& color);
    void DrawString(int16_t x, int16_t y, const std::string& str, const Color& color);

    void Clear(const Color& color = Color(0, 0, 0));
    void Refresh();

    FrameBuffer& GetFrameBuffer() { return fb_; }

private:
    void DrawPixelClipped(int16_t x, int16_t y, const Color& color);

    FrameBuffer& fb_;
    MatrixMapper& mapper_;
    LedStripDriver& driver_;
    Font& font_;
};
