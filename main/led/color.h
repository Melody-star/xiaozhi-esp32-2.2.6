#pragma once
#include <cstdint>

struct Color {
    uint8_t r = 0, g = 0, b = 0;

    Color() = default;
    Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};
