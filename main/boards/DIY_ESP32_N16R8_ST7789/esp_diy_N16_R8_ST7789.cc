/**
 * @file esp_diy_N16_R8_ST7789.cc
 */

#include "dual_network_board.h" 
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "my_controller_gpio.h"
#include "led/gpio_led.h"
#include "assets/lang_config.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>

#include "weather_controller.h"
#include "servo_controller.h"
#include <driver/uart.h>

#define TAG "EspDiyBoard"

class EspDiyBoard : public DualNetworkBoard {
private:
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

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        ESP_LOGI(TAG, "Инициализация ST7789 1.9\"");

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RESET_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
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

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            volume = (volume > 100) ? 100 : volume;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
    }

    void InitializeTools() {
        static Controller_GPIO solder(POWER_OUT_CH2_GPIO, "solder", "Паяльник");
        static Controller_GPIO dryer(POWER_OUT_CH1_GPIO, "dryer", "Фен");
        static WeatherController weather;

        servo_1 = new ServoController(SERVO_CH1_GPIO, "Servo1", "Сервопривод 1", LEDC_CHANNEL_0);
        servo_2 = new ServoController(SERVO_CH2_GPIO, "Servo2", "Сервопривод 2", LEDC_CHANNEL_1);
        servo_3 = new ServoController(SERVO_CH3_GPIO, "Servo3", "Сервопривод 3", LEDC_CHANNEL_2);
        servo_4 = new ServoController(SERVO_CH4_GPIO, "Servo4", "Сервопривод 4", LEDC_CHANNEL_3, true);
        servo_5 = new ServoController(SERVO_CH5_GPIO, "Servo5", "Сервопривод 5", LEDC_CHANNEL_4, true);

        ESP_LOGI(TAG, "MCP Tools и сервоприводы зарегистрированы");
    }

public:
    EspDiyBoard() : DualNetworkBoard(MODEM_UART_TX_PIN, MODEM_UART_RX_PIN),
        boot_button_(BOOT_BUTTON_GPIO, false, 2000, 250),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {

        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeTools();

        GetBacklight()->SetBrightness(100);   // Включаем подсветку

        Settings mqtt_settings("mqtt", true);
        mqtt_settings.SetInt("keepalive", 60);
        mqtt_settings.SetInt("reconnect_timeout", 8);

        ESP_LOGI(TAG, "Плата с ST7789 1.9\" инициализирована");
        ESP_LOGI(TAG, "Heap after init: %d bytes free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    virtual void StartNetwork() override {
        auto net_type = GetNetworkType();

        if (net_type == NetworkType::WIFI) {
            ESP_LOGW(TAG, "=== Переход на Wi-Fi ===");
        } else if (net_type == NetworkType::ML307) {
            ESP_LOGI(TAG, "=== Переход на 4G ===");
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        DualNetworkBoard::StartNetwork();

        if (net_type == NetworkType::ML307) {
            ESP_LOGI(TAG, "ML307: ждём 3 секунды");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        auto net_type = GetNetworkType();

        if (net_type == NetworkType::WIFI) {
            DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
            ESP_LOGI(TAG, "Wi-Fi: PERFORMANCE");
        } else {
            DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel::BALANCED);
            ESP_LOGI(TAG, "ML307: BALANCED");
        }
    }

    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO, 0);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, false);
        return &backlight;
    }
};

DECLARE_BOARD(EspDiyBoard);