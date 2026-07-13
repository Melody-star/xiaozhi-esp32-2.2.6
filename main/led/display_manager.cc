#include "display_manager.h"
#include "graphics.h"
#include "font.h"

#include <esp_log.h>
#include <time.h>
#include <cstdio>

#define TAG "DisplayManager"

DisplayManager::DisplayManager(Graphics& graphics, Font& font)
    : graphics_(graphics), font_(font) {

    Tick();

    esp_timer_create_args_t args = {};
    args.callback = OnTimer;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "display_tick";
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_));
    esp_timer_start_periodic(timer_, 1000000);
}

DisplayManager::~DisplayManager() {
    if (timer_) {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}

void DisplayManager::OnTimer(void* arg) {
    static_cast<DisplayManager*>(arg)->Tick();
}

int DisplayManager::TextWidth(const std::string& text) const {
    int w = 0;
    for (char c : text) w += font_.CharAdvance(c);
    return w;
}

void DisplayManager::NextMode() {
    switch (mode_) {
    case DisplayMode::CLOCK:
        mode_ = DisplayMode::TEMPERATURE;
        break;
    case DisplayMode::TEMPERATURE:
        mode_ = DisplayMode::HUMIDITY;
        break;
    case DisplayMode::HUMIDITY:
        mode_ = DisplayMode::DATE;
        break;
    case DisplayMode::DATE:
        mode_ = DisplayMode::CLOCK;
        break;
    }
    mode_switch_time_ = esp_timer_get_time();
    Tick();
}

DisplayMode DisplayManager::GetCurrentMode() const {
    return mode_;
}

void DisplayManager::SetTemperature(float temp) {
    temperature_ = temp;
}

void DisplayManager::SetHumidity(float hum) {
    humidity_ = hum;
}

void DisplayManager::Tick() {
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    if (t.tm_year <= 124) return;
    if (t.tm_sec == last_second_) return;
    last_second_ = t.tm_sec;

    if (mode_ != DisplayMode::CLOCK) {
        int64_t elapsed = (esp_timer_get_time() - mode_switch_time_) / 1000000;
        if (elapsed >= 5) {
            mode_ = DisplayMode::CLOCK;
        }
    }

    switch (mode_) {
    case DisplayMode::CLOCK:
        DrawClock();
        break;
    case DisplayMode::TEMPERATURE:
        DrawTemperature();
        break;
    case DisplayMode::HUMIDITY:
        DrawHumidity();
        break;
    case DisplayMode::DATE:
        DrawDate();
        break;
    }
}

void DisplayManager::DrawClock() {
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    graphics_.Clear();
    Color c(0, 120, 255);

    auto d = [&](int x, char ch) { graphics_.DrawChar(x, 1, ch, c); };
    d(2,  '0' + t.tm_hour / 10);
    d(6,  '0' + t.tm_hour % 10);
    d(10, ':');
    d(12, '0' + t.tm_min  / 10);
    d(16, '0' + t.tm_min  % 10);
    d(20, ':');
    d(22, '0' + t.tm_sec  / 10);
    d(26, '0' + t.tm_sec  % 10);

    int today = t.tm_wday == 0 ? 6 : t.tm_wday - 1;
    for (int i = 0; i < 7; i++) {
        int bar_x = 2 + i * 4;
        Color bar_color = (i == today) ? Color(255, 0, 0) : Color(0, 180, 0);
        graphics_.FillRect(bar_x, 7, 3, 1, bar_color);
    }

    graphics_.Refresh();
}

void DisplayManager::DrawTemperature() {
    char buf[16];
    snprintf(buf, sizeof(buf), "T %.1f", temperature_);
    ShowText(buf);
}

void DisplayManager::DrawHumidity() {
    char buf[16];
    snprintf(buf, sizeof(buf), "HUM %.0f", humidity_);
    ShowText(buf);
}

void DisplayManager::DrawDate() {
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    char buf[16];
    static const char* months[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    snprintf(buf, sizeof(buf), "%d %s", t.tm_mday, months[t.tm_mon]);
    ShowText(buf);
}

void DisplayManager::ShowText(const std::string& text) {
    graphics_.Clear();
    int w = TextWidth(text);
    int x = (32 - w) / 2;
    graphics_.DrawString(x, 1, text, Color(0, 120, 255));
    graphics_.Refresh();
}
