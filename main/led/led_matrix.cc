#include "led_matrix.h"
#include "strip_driver.h"
#include "matrix_mapper.h"
#include "frame_buffer.h"
#include "font.h"
#include "graphics.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <mutex>
#include <functional>
#include <time.h>

#define TAG "LedMatrix"

LedMatrix::LedMatrix(gpio_num_t gpio, uint16_t width, uint16_t height) {
    strip_driver_.reset(new LedStripDriver());
    strip_driver_->Init(gpio, width * height);

    mapper_.reset(new MatrixMapper(width, height, MatrixLayout::TILED, 8, 8));
    framebuffer_.reset(new FrameBuffer(width, height));
    font_.reset(new Font());
    graphics_.reset(new Graphics(*framebuffer_, *mapper_, *strip_driver_, *font_));

    auto timer_cb = [](void* arg) {
        auto self = static_cast<LedMatrix*>(arg);
        if (self->scroll_callback_) {
            self->scroll_callback_();
        }
    };
    esp_timer_create_args_t args = {};
    args.callback = timer_cb;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "matrix_scroll";
    ESP_ERROR_CHECK(esp_timer_create(&args, &scroll_timer_));
}

LedMatrix::~LedMatrix() {
    esp_timer_stop(scroll_timer_);
    esp_timer_delete(scroll_timer_);
}

void LedMatrix::OnStateChanged() {
}

void LedMatrix::StartScrollTask(int interval_ms, std::function<void()> cb) {
    esp_timer_stop(scroll_timer_);
    scroll_callback_ = cb;
    esp_timer_start_periodic(scroll_timer_, interval_ms * 1000);
}

void LedMatrix::Marquee(int speed_ms) {
    scroll_offset_ = 0;
    auto fb_w = framebuffer_->Width();
    auto fb_h = framebuffer_->Height();
    int total = fb_w * fb_h;

    StartScrollTask(speed_ms, [this, total, fb_w]() {
        if (scroll_offset_ >= total) {
            framebuffer_->Clear();
            scroll_offset_ = 0;
        }

        uint16_t x = scroll_offset_ % fb_w;
        uint16_t y = scroll_offset_ / fb_w;
        graphics_->DrawPixel(x, y, Color(0, 80, 255));
        graphics_->Refresh();

        scroll_offset_++;
    });
}

void LedMatrix::ScrollText(const std::string& text, int speed_ms) {
    scroll_offset_ = 0;
    int text_width = 0;
    for (char c : text) text_width += font_->CharAdvance(c);
    int total_width = text_width + 8;

    StartScrollTask(speed_ms, [this, text, total_width]() {
        if (scroll_offset_ >= total_width) {
            scroll_offset_ = -8;
        }

        framebuffer_->Clear();
        graphics_->DrawString(-scroll_offset_, 2, text, Color(0, 80, 255));
        graphics_->Refresh();

        scroll_offset_++;
    });
}

void LedMatrix::ShowText(const std::string& text) {
    esp_timer_stop(scroll_timer_);
    framebuffer_->Clear();
    int text_width = 0;
    for (char c : text) text_width += font_->CharAdvance(c);
    int x = (framebuffer_->Width() - text_width) / 2;
    graphics_->DrawString(x, 1, text, Color(0, 80, 255));
    graphics_->Refresh();
}

void LedMatrix::StopScroll() {
    esp_timer_stop(scroll_timer_);
}
