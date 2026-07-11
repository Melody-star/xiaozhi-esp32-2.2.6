#ifndef _LED_MATRIX_H_
#define _LED_MATRIX_H_

#include "led.h"
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_timer.h>
#include <mutex>
#include <vector>
#include <string>

struct RgbColor {
    uint8_t r = 0, g = 0, b = 0;
};

class LedMatrix : public Led {
public:
    LedMatrix(gpio_num_t gpio, int width, int height);
    virtual ~LedMatrix();

    void OnStateChanged() override;

    void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void SetAll(uint8_t r, uint8_t g, uint8_t b);
    void SetBrightness(uint8_t brightness);
    void Clear();
    void Show();

    void Marquee(int speed_ms = 80);
    void StopScroll();

private:
    int width_, height_;
    int num_leds_;
    std::mutex mutex_;
    led_strip_handle_t led_strip_ = nullptr;
    std::vector<RgbColor> pixels_;
    esp_timer_handle_t scroll_timer_ = nullptr;
    std::function<void()> scroll_callback_ = nullptr;
    int scroll_offset_ = 0;

    int Index(int x, int y);
    void DrawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b);
    void DrawColon(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void StartScrollTask(int interval_ms, std::function<void()> cb);
};

#endif
