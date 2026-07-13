#include "frame_buffer.h"
#include <cstdlib>
#include <cstring>

FrameBuffer::FrameBuffer(uint16_t width, uint16_t height)
    : width_(width), height_(height) {
    buffer_ = new Color[width_ * height_]();
}

FrameBuffer::~FrameBuffer() {
    delete[] buffer_;
}

void FrameBuffer::Clear(const Color& color) {
    for (uint32_t i = 0; i < (uint32_t)width_ * height_; i++) {
        buffer_[i] = color;
    }
}

void FrameBuffer::SetPixel(uint16_t x, uint16_t y, const Color& color) {
    if (x >= width_ || y >= height_) return;
    buffer_[y * width_ + x] = color;
}

Color FrameBuffer::GetPixel(uint16_t x, uint16_t y) const {
    if (x >= width_ || y >= height_) return Color(0, 0, 0);
    return buffer_[y * width_ + x];
}

Color* FrameBuffer::row(uint16_t y) const {
    return buffer_ + y * width_;
}
