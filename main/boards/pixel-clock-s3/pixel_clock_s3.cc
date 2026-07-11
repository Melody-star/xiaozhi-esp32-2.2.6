#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "PixelClockS3"

#define AHT20_ADDR 0x38

class Aht20 {
public:
    Aht20(i2c_master_bus_handle_t i2c_bus) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AHT20_ADDR,
            .scl_speed_hz = 400 * 1000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &device_));
        uint8_t init_cmd[] = {0xBE, 0x08, 0x00};
        i2c_master_transmit(device_, init_cmd, sizeof(init_cmd), 100);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool Read(float& temperature, float& humidity) {
        uint8_t trig_cmd[] = {0xAC, 0x33, 0x00};
        if (i2c_master_transmit(device_, trig_cmd, sizeof(trig_cmd), 100) != ESP_OK) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(80));
        uint8_t data[6];
        if (i2c_master_receive(device_, data, sizeof(data), 100) != ESP_OK) {
            return false;
        }
        if ((data[0] & 0x80) != 0) {
            return false;
        }
        uint32_t raw_hum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
        uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
        humidity = (raw_hum * 100.0f) / (1 << 20);
        temperature = (raw_temp * 200.0f) / (1 << 20) - 50.0f;
        return true;
    }

private:
    i2c_master_dev_handle_t device_;
};

class PixelClockS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Aht20* aht20_;
    adc_oneshot_unit_handle_t adc_handle_;
    Button boot_button_;

    void InitializeI2c() {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AHT20_I2C_SDA_PIN,
            .scl_io_num = AHT20_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));
        aht20_ = new Aht20(i2c_bus_);
    }

    void InitializeAdc() {
        adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle_));
        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_1, &chan_cfg));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.environment.get_temperature_humidity",
            "获取当前环境的温度和湿度",
            PropertyList(),
            [this](const PropertyList& props) -> ReturnValue {
                float temp, hum;
                if (aht20_->Read(temp, hum)) {
                    char json[128];
                    snprintf(json, sizeof(json), "{\"temperature\": %.1f, \"humidity\": %.1f}", temp, hum);
                    return std::string(json);
                }
                return std::string("{\"error\": \"sensor read failed\"}");
            });

        mcp.AddTool("self.environment.get_light_level",
            "获取当前环境亮度级别 (0-4095，值越大越亮)",
            PropertyList(),
            [this](const PropertyList& props) -> ReturnValue {
                int raw_value;
                if (adc_oneshot_read(adc_handle_, ADC_CHANNEL_1, &raw_value) == ESP_OK) {
                    return raw_value;
                }
                return std::string("{\"error\": \"adc read failed\"}");
            });
    }

public:
    PixelClockS3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeAdc();
        InitializeButtons();
        InitializeTools();
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }
};

DECLARE_BOARD(PixelClockS3Board);
