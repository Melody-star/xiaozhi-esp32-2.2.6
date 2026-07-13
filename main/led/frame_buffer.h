#pragma once
#include "color.h"
#include <cstdint>

class FrameBuffer {
public:
    FrameBuffer(uint16_t width, uint16_t height);
    ~FrameBuffer();

    void Clear(const Color& color = Color(0, 0, 0));

    void SetPixel(uint16_t x, uint16_t y, const Color& color);
    Color GetPixel(uint16_t x, uint16_t y) const;

    Color* operator[](uint16_t y) { return row(y); }
    const Color* operator[](uint16_t y) const { return row(y); }

    Color* Row(uint16_t y) { return row(y); }
    const Color* Row(uint16_t y) const { return row(y); }

    uint16_t Width() const { return width_; }
    uint16_t Height() const { return height_; }

private:
    Color* row(uint16_t y) const;

    uint16_t width_;
    uint16_t height_;
    Color* buffer_;
};
