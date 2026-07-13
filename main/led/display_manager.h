#pragma once
#include "color.h"
#include <cstdint>
#include <esp_timer.h>
#include <memory>
#include <string>

class Font;
class Graphics;

enum class DisplayMode : uint8_t {
    CLOCK,
    TEMPERATURE,
    HUMIDITY,
    DATE,
};

class DisplayManager {
public:
    DisplayManager(Graphics& graphics, Font& font);
    ~DisplayManager();

    void NextMode();
    DisplayMode GetCurrentMode() const;

    void SetTemperature(float temp);
    void SetHumidity(float hum);

private:
    static void OnTimer(void* arg);
    void Tick();
    int TextWidth(const std::string& text) const;

    void DrawClock();
    void DrawTemperature();
    void DrawHumidity();
    void DrawDate();
    void ShowText(const std::string& text);

    Graphics& graphics_;
    Font& font_;
    esp_timer_handle_t timer_ = nullptr;
    DisplayMode mode_ = DisplayMode::CLOCK;
    float temperature_ = 0.0f;
    float humidity_ = 0.0f;
    int last_second_ = -1;
    int64_t mode_switch_time_ = 0;
};
