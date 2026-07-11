#include "led_matrix.h"
#include <esp_log.h>
#include <algorithm>
#include <cstring>

#define TAG "LedMatrix"

static const uint8_t FONT_5X7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
};

LedMatrix::LedMatrix(gpio_num_t gpio, int width, int height)
    : width_(width), height_(height), num_leds_(width * height) {
    pixels_.resize(num_leds_);

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = num_leds_;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);

    esp_timer_create_args_t timer_args = {
        .callback = [](void *arg) {
            auto self = static_cast<LedMatrix*>(arg);
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (self->scroll_callback_) {
                self->scroll_callback_();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "matrix_scroll",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &scroll_timer_));
}

LedMatrix::~LedMatrix() {
    esp_timer_stop(scroll_timer_);
    esp_timer_delete(scroll_timer_);
    if (led_strip_) {
        led_strip_del(led_strip_);
    }
}

void LedMatrix::OnStateChanged() {
}

int LedMatrix::Index(int x, int y) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return -1;
    if (y % 2 == 0) {
        return y * width_ + x;
    } else {
        return y * width_ + (width_ - 1 - x);
    }
}

void LedMatrix::SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    int idx = Index(x, y);
    if (idx < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(scroll_timer_);
    pixels_[idx] = {r, g, b};
    led_strip_set_pixel(led_strip_, idx, r, g, b);
}

void LedMatrix::SetAll(uint8_t r, uint8_t g, uint8_t b) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(scroll_timer_);
    for (int i = 0; i < num_leds_; i++) {
        pixels_[i] = {r, g, b};
        led_strip_set_pixel(led_strip_, i, r, g, b);
    }
    led_strip_refresh(led_strip_);
}

void LedMatrix::SetBrightness(uint8_t brightness) {
}

void LedMatrix::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(scroll_timer_);
    led_strip_clear(led_strip_);
    for (auto& p : pixels_) {
        p = {0, 0, 0};
    }
}

void LedMatrix::Show() {
    std::lock_guard<std::mutex> lock(mutex_);
    led_strip_refresh(led_strip_);
}

void LedMatrix::DrawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b) {
    if (c < '0' || c > '9') return;
    int idx = c - '0';
    for (int col = 0; col < 5; col++) {
        uint8_t bits = FONT_5X7[idx][col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                int px = x + col;
                int py = y + row;
                int i = Index(px, py);
                if (i >= 0) {
                    pixels_[i] = {r, g, b};
                    led_strip_set_pixel(led_strip_, i, r, g, b);
                }
            }
        }
    }
}

void LedMatrix::DrawColon(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    for (int row = 0; row < 7; row++) {
        if (row == 1 || row == 2 || row == 4 || row == 5) {
            int i = Index(x, y + row);
            if (i >= 0) {
                pixels_[i] = {r, g, b};
                led_strip_set_pixel(led_strip_, i, r, g, b);
            }
        }
    }
}

void LedMatrix::StartScrollTask(int interval_ms, std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(scroll_timer_);
    scroll_callback_ = cb;
    esp_timer_start_periodic(scroll_timer_, interval_ms * 1000);
}

void LedMatrix::Marquee(int speed_ms) {
    scroll_offset_ = 0;
    StartScrollTask(speed_ms, [this]() {
        if (scroll_offset_ >= num_leds_) {
            led_strip_clear(led_strip_);
            for (auto& p : pixels_) p = {0, 0, 0};
            scroll_offset_ = 0;
        }

        uint8_t r = 0, g = 80, b = 255;
        led_strip_set_pixel(led_strip_, scroll_offset_, r, g, b);
        pixels_[scroll_offset_] = {r, g, b};

        led_strip_refresh(led_strip_);
        scroll_offset_++;
    });
}

void LedMatrix::StopScroll() {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(scroll_timer_);
}
