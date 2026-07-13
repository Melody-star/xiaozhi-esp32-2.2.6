#pragma once
#include <cstdint>
#include <cstddef>

struct Glyph {
    uint8_t width;
    uint8_t height;
    uint8_t x_advance;
};

class Font {
public:
    Font();

    const Glyph* GetGlyph(char c) const;
    bool HasChar(char c) const;
    uint8_t CharAdvance(char c) const;
    const uint8_t* GetBitmap(char c) const;
};
