#include "matrix_mapper.h"

MatrixMapper::MatrixMapper(uint16_t width, uint16_t height,
                           MatrixLayout layout, uint16_t tile_w, uint16_t tile_h)
    : width_(width), height_(height), layout_(layout) {
    if (tile_w == 0) tile_w_ = width_;
    else tile_w_ = tile_w;
    if (tile_h == 0) tile_h_ = height_;
    else tile_h_ = tile_h;
}

uint16_t MatrixMapper::XY(uint16_t x, uint16_t y) const {
    if (x >= width_ || y >= height_) return 0xFFFF;

    switch (layout_) {
    case MatrixLayout::NORMAL:
        return y * width_ + x;

    case MatrixLayout::SERPENTINE:
        if (y % 2 == 0) {
            return y * width_ + x;
        } else {
            return y * width_ + (width_ - 1 - x);
        }

    case MatrixLayout::TILED: {
        uint16_t tile_col = x / tile_w_;
        uint16_t local_x = x % tile_w_;
        return tile_col * tile_w_ * tile_h_ + y * tile_w_ + local_x;
    }

    default:
        return y * width_ + x;
    }
}
