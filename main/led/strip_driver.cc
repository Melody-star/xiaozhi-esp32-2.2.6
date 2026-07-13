#include "strip_driver.h"
#include <esp_log.h>

static const char* TAG = "StripDriver";

LedStripDriver::~LedStripDriver() {
    if (strip_) {
        led_strip_del(strip_);
    }
}

bool LedStripDriver::Init(gpio_num_t gpio, uint16_t num_leds) {
    if (initialized_) return true;

    led_strip_config_t config = {};
    config.strip_gpio_num = gpio;
    config.max_leds = num_leds;
    config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt = {};
    rmt.resolution_hz = 10 * 1000 * 1000;

    esp_err_t err = led_strip_new_rmt_device(&config, &rmt, &strip_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return false;
    }

    num_leds_ = num_leds;
    initialized_ = true;
    Clear();
    return true;
}

bool LedStripDriver::SetPixel(uint16_t index, const Color& color) {
    if (!initialized_ || index >= num_leds_) return false;
    return led_strip_set_pixel(strip_, index, color.g, color.r, color.b) == ESP_OK;
}

bool LedStripDriver::Refresh() {
    if (!initialized_) return false;
    return led_strip_refresh(strip_) == ESP_OK;
}

bool LedStripDriver::Clear() {
    if (!initialized_) return false;
    return led_strip_clear(strip_) == ESP_OK;
}
