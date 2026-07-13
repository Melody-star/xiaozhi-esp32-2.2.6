#include "font.h"

// 3x5 font — one byte per column, bit0=row0(top) .. bit4=row4(bottom)
// Index: 0-9=digits, 10=colon, 11-36=A-Z, 37=period, 38=space
static const uint8_t data_[39][3] = {
    {0x1F, 0x11, 0x1F}, // 0
    {0x12, 0x1F, 0x10}, // 1
    {0x1D, 0x15, 0x17}, // 2
    {0x15, 0x15, 0x1F}, // 3
    {0x07, 0x04, 0x1F}, // 4
    {0x17, 0x15, 0x1D}, // 5
    {0x1F, 0x15, 0x1D}, // 6
    {0x01, 0x01, 0x1F}, // 7
    {0x1F, 0x15, 0x1F}, // 8
    {0x17, 0x15, 0x1F}, // 9
    {0x0A, 0x00, 0x00}, // :
    {0x1E, 0x05, 0x1E}, // A
    {0x1F, 0x15, 0x1F}, // B
    {0x1F, 0x11, 0x11}, // C
    {0x1F, 0x11, 0x0E}, // D
    {0x1F, 0x15, 0x15}, // E
    {0x1F, 0x05, 0x05}, // F
    {0x1F, 0x11, 0x1D}, // G
    {0x1F, 0x04, 0x1F}, // H
    {0x11, 0x1F, 0x11}, // I
    {0x08, 0x10, 0x1F}, // J
    {0x1F, 0x04, 0x1A}, // K
    {0x1F, 0x10, 0x10}, // L
    {0x1F, 0x02, 0x1F}, // M
    {0x1F, 0x06, 0x1F}, // N
    {0x1F, 0x11, 0x1F}, // O
    {0x1F, 0x05, 0x07}, // P
    {0x0F, 0x19, 0x17}, // Q
    {0x1F, 0x05, 0x1A}, // R
    {0x17, 0x15, 0x1D}, // S
    {0x01, 0x1F, 0x01}, // T
    {0x1F, 0x10, 0x1F}, // U
    {0x07, 0x18, 0x07}, // V
    {0x1F, 0x08, 0x1F}, // W
    {0x1B, 0x04, 0x1B}, // X
    {0x03, 0x1C, 0x03}, // Y
    {0x19, 0x15, 0x13}, // Z
    {0x10, 0x00, 0x00}, // .
    {0x00, 0x00, 0x00}, // (space)
};

static const Glyph glyphs_[39] = {
    {3, 5, 4}, // 0
    {3, 5, 4}, // 1
    {3, 5, 4}, // 2
    {3, 5, 4}, // 3
    {3, 5, 4}, // 4
    {3, 5, 4}, // 5
    {3, 5, 4}, // 6
    {3, 5, 4}, // 7
    {3, 5, 4}, // 8
    {3, 5, 4}, // 9
    {1, 5, 2}, // :
    {3, 5, 4}, // A
    {3, 5, 4}, // B
    {3, 5, 4}, // C
    {3, 5, 4}, // D
    {3, 5, 4}, // E
    {3, 5, 4}, // F
    {3, 5, 4}, // G
    {3, 5, 4}, // H
    {3, 5, 4}, // I
    {3, 5, 4}, // J
    {3, 5, 4}, // K
    {3, 5, 4}, // L
    {3, 5, 4}, // M
    {3, 5, 4}, // N
    {3, 5, 4}, // O
    {3, 5, 4}, // P
    {3, 5, 4}, // Q
    {3, 5, 4}, // R
    {3, 5, 4}, // S
    {3, 5, 4}, // T
    {3, 5, 4}, // U
    {3, 5, 4}, // V
    {3, 5, 4}, // W
    {3, 5, 4}, // X
    {3, 5, 4}, // Y
    {3, 5, 4}, // Z
    {1, 5, 2}, // .
    {3, 5, 4}, // (space)
};

Font::Font() {}

const Glyph* Font::GetGlyph(char c) const {
    if (c >= '0' && c <= '9') return &glyphs_[c - '0'];
    if (c == ':') return &glyphs_[10];
    if (c >= 'A' && c <= 'Z') return &glyphs_[c - 'A' + 11];
    if (c == '.') return &glyphs_[37];
    if (c == ' ') return &glyphs_[38];
    return nullptr;
}

bool Font::HasChar(char c) const {
    return (c >= '0' && c <= '9') || c == ':' || (c >= 'A' && c <= 'Z') || c == '.' || c == ' ';
}

uint8_t Font::CharAdvance(char c) const {
    if (c == ':' || c == '.') return 2;
    return 4;
}

const uint8_t* Font::GetBitmap(char c) const {
    if (c >= '0' && c <= '9') return data_[c - '0'];
    if (c == ':') return data_[10];
    if (c >= 'A' && c <= 'Z') return data_[c - 'A' + 11];
    if (c == '.') return data_[37];
    if (c == ' ') return data_[38];
    return nullptr;
}
