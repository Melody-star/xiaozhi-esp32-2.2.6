#pragma once
#include <cstdint>

enum class MatrixLayout : uint8_t {
    NORMAL,
    SERPENTINE,
    TILED,
};

class MatrixMapper {
public:
    MatrixMapper(uint16_t width, uint16_t height,
                 MatrixLayout layout = MatrixLayout::SERPENTINE,
                 uint16_t tile_w = 0, uint16_t tile_h = 0);

    uint16_t XY(uint16_t x, uint16_t y) const;

    void SetLayout(MatrixLayout layout) { layout_ = layout; }
    MatrixLayout GetLayout() const { return layout_; }

    uint16_t Width() const { return width_; }
    uint16_t Height() const { return height_; }

private:
    uint16_t width_;
    uint16_t height_;
    MatrixLayout layout_;
    uint16_t tile_w_;
    uint16_t tile_h_;
};
