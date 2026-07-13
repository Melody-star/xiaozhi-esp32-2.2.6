#pragma once
#include "led.h"
#include "color.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <memory>
#include <string>
#include <functional>

class LedStripDriver;
class MatrixMapper;
class FrameBuffer;
class Font;
class Graphics;

class LedMatrix : public Led {
public:
    LedMatrix(gpio_num_t gpio, uint16_t width, uint16_t height);
    virtual ~LedMatrix();

    void OnStateChanged() override;

    void Marquee(int speed_ms = 80);
    void ScrollText(const std::string& text, int speed_ms = 80);
    void ShowText(const std::string& text);
    void StopScroll();

    Graphics& GetGraphics() { return *graphics_; }
    Font& GetFont() { return *font_; }

private:
    void StartScrollTask(int interval_ms, std::function<void()> cb);

    std::unique_ptr<LedStripDriver> strip_driver_;
    std::unique_ptr<MatrixMapper> mapper_;
    std::unique_ptr<FrameBuffer> framebuffer_;
    std::unique_ptr<Font> font_;
    std::unique_ptr<Graphics> graphics_;

    esp_timer_handle_t scroll_timer_ = nullptr;
    std::function<void()> scroll_callback_ = nullptr;
    int scroll_offset_ = 0;
};
