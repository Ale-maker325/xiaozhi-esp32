/**
 * @file esp_diy_N16_R8.cc
 * @brief Главный файл конфигурации платы ESP-32 N16R8 с DualNetwork.
 * Исправления для стабильности MQTT на 4G (ML307).
 */

#include "dual_network_board.h" 
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "my_controller_gpio.h"
#include "led/gpio_led.h"
#include "assets/lang_config.h"
#include "settings.h"               // ← ОБЯЗАТЕЛЬНО для настройки MQTT

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "weather_controller.h"
#include "servo_controller.h"

#include <driver/uart.h>

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "EspDiyBoard"

class EspDiyBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    ServoController* servo_1 = nullptr;
    ServoController* servo_2 = nullptr;
    ServoController* servo_3 = nullptr;
    ServoController* servo_4 = nullptr;
    ServoController* servo_5 = nullptr;

    bool is_double_clicking_ = false;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = { .dc_low_on_data = 0, .disable_control_phase = 0 },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = { .height = static_cast<uint8_t>(DISPLAY_HEIGHT) };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnDoubleClick([this]() {
            is_double_clicking_ = true;
            ESP_LOGI(TAG, "Double click: Switching network...");
            SwitchNetworkType();
        });

        boot_button_.OnClick([this]() {
            vTaskDelay(pdMS_TO_TICKS(300));
            if (is_double_clicking_) {
                is_double_clicking_ = false;
                return;
            }
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI && app.GetDeviceState() == kDeviceStateStarting) {
                auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                wifi_board.EnterWifiConfigMode();
            } else {
                app.ToggleChatState();
            }
        });

        touch_button_.OnPressDown([this]() { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]() { Application::GetInstance().StopListening(); });

        // Volume buttons (оставил как у тебя)
        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            volume = (volume > 100) ? 100 : volume;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        // ... остальные volume handlers (добавь сам если нужно)
    }

    void InitializeTools() {
        static Controller_GPIO solder(POWER_OUT_CH2_GPIO, "solder", "Паяльник");
        static Controller_GPIO dryer(POWER_OUT_CH1_GPIO, "dryer", "Фен");
        static WeatherController weather;

        servo_1 = new ServoController(SERVO_CH1_GPIO, "Servo 1",   "сервопривод 1",  LEDC_CHANNEL_0);
        servo_2 = new ServoController(SERVO_CH2_GPIO, "Servo 2",   "сервопривод 2",  LEDC_CHANNEL_1);
        servo_3 = new ServoController(SERVO_CH3_GPIO, "Servo 3",   "сервопривод 3",  LEDC_CHANNEL_2);
        servo_4 = new ServoController(SERVO_CH4_GPIO, "Servo 4",   "сервопривод 4",  LEDC_CHANNEL_3, true);
        servo_5 = new ServoController(SERVO_CH5_GPIO, "Servo 5",   "сервопривод 5",  LEDC_CHANNEL_4, true);

        ESP_LOGI(TAG, "MCP Tools зарегистрированы");
    }

public:
    EspDiyBoard() : DualNetworkBoard(MODEM_UART_TX_PIN, MODEM_UART_RX_PIN, GPIO_NUM_NC),
        boot_button_(BOOT_BUTTON_GPIO, false, 2000, 250),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {

        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();

        // Настройка MQTT
        Settings mqtt_settings("mqtt", true);
        mqtt_settings.SetInt("keepalive", 60);
        mqtt_settings.SetInt("reconnect_timeout", 8);
        ESP_LOGI(TAG, "MQTT settings forced: keepalive=60s");

        ESP_LOGI(TAG, "Плата инициализирована");
        // Мониторинг памяти
        ESP_LOGI(TAG, "Heap after init: %d bytes free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }



    // virtual void StartNetwork() override {
    //     DualNetworkBoard::StartNetwork();

    //     if (GetNetworkType() == NetworkType::ML307) {
    //         ESP_LOGI(TAG, "ML307: задержка 3 сек для стабилизации PPP");
    //         vTaskDelay(pdMS_TO_TICKS(3000));
    //     } else {
    //         ESP_LOGI(TAG, "Wi-Fi: запуск");
    //     }
    // }

        virtual void StartNetwork() override {
        auto net_type = GetNetworkType();

        if (net_type == NetworkType::WIFI) {
            ESP_LOGW(TAG, "=== Переход на Wi-Fi: полностью выключаем ML307 ===");
            
            // Если есть PWRKEY — выключаем модем
            // gpio_set_level(MODEM_PWR_PIN, 0);     // раскомментируй, если пин определён

            ESP_LOGW(TAG, "Модем ML307 выключен.");
        } 
        else if (net_type == NetworkType::ML307) {
            ESP_LOGI(TAG, "=== Переход на 4G: включаем/инициализируем модем ===");
            
            // Включаем модем аппаратно (PWRKEY) PWRKEY обычно требует импульс (нажать на 1–2 секунды), а не постоянный уровень.
            //gpio_set_level(MODEM_PWR_PIN, 1);
            //vTaskDelay(pdMS_TO_TICKS(1500));  // импульс 1.5 секунды
            //gpio_set_level(MODEM_PWR_PIN, 0);
            
            
            // Небольшая задержка перед инициализацией
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        // Основная инициализация (DualNetworkBoard)
        DualNetworkBoard::StartNetwork();

        // Дополнительная задержка только для 4G
        if (net_type == NetworkType::ML307) {
            ESP_LOGI(TAG, "ML307: ждём стабилизации сети (3 секунды)");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }



    /** Самое важное исправление — баланс энергосбережения */
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        auto net_type = GetNetworkType();

        if (net_type == NetworkType::WIFI) {
            // Для Wi-Fi — PERFORMANCE (самый стабильный для MQTT в твоей сборке)
            DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
            ESP_LOGI(TAG, "Wi-Fi: PERFORMANCE forced для стабильности MQTT");
        } else {
            // Для 4G — BALANCED (чтобы модем не перегревался)
            DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel::BALANCED);
            ESP_LOGI(TAG, "ML307: BALANCED power save");
        }
    }



    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO, 0);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
#endif
        return nullptr;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(EspDiyBoard);