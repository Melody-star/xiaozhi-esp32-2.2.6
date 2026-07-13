#pragma once
#include "color.h"
#include <driver/gpio.h>
#include <led_strip.h>

class LedStripDriver {
public:
    LedStripDriver() = default;
    ~LedStripDriver();

    bool Init(gpio_num_t gpio, uint16_t num_leds);
    bool SetPixel(uint16_t index, const Color& color);
    bool Refresh();
    bool Clear();
    uint16_t NumLeds() const { return num_leds_; }

private:
    led_strip_handle_t strip_ = nullptr;
    uint16_t num_leds_ = 0;
    bool initialized_ = false;
};
