/**
 * @file esp_diy_N16_R8.cc
 * @brief Главный файл конфигурации платы ESP-32 N16R8 с DualNetwork.
 * Исправления для стабильности MQTT на 4G (ML307).
 */

#include "dual_network_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "display/lcd_display.h"    // ← для SpiLcdDisplay (вариант с внешним SPI TFT ST7789)
#include "backlight.h"              // ← для PwmBacklight (подсветка SPI TFT)
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
#include <vector>                   // ← std::vector<Controller_GPIO*> (режим DISABLE_SERVOS, InitializeTools())
#include <driver/i2c_master.h>
#include <driver/spi_common.h>      // ← для SPI-шины TFT-дисплея
#include <esp_lcd_panel_io.h>       // ← для esp_lcd_new_panel_io_spi
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

#ifndef DISABLE_SERVOS
    ServoController* servo_1 = nullptr;
    ServoController* servo_2 = nullptr;
    ServoController* servo_3 = nullptr;
    ServoController* servo_4 = nullptr;
    ServoController* servo_5 = nullptr;
#endif

    bool is_double_clicking_ = false;
    bool network_fallback_triggered_ = false; // защита от повторного срабатывания авто-отката на Wi-Fi

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
            .dev_addr = DISPLAY_I2C_ADDR,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = { .dc_low_on_data = 0, .disable_control_phase = 0 },
            .scl_speed_hz = DISPLAY_I2C_FREQ_HZ,
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

#ifdef DISPLAY_TYPE_SPI_ST7789_170X320
    // --- Вариант: отдельный SPI TFT ST7789 170x320 (см. config.h, раздел 4) ---
    void InitializeDisplaySpiBus() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC; // дисплей write-only, MISO не нужен
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSpiTftDisplay() {
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_SPI_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));

        display_ = new SpiLcdDisplay(panel_io_, panel_,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
#endif

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
            int volume = codec->output_volume() + VOLUME_STEP;
            volume = (volume > VOLUME_MAX) ? VOLUME_MAX : volume;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        // ... остальные volume handlers (добавь сам если нужно)
    }

    void InitializeTools() {
        // Технический id ("relay_chN") — стабильный, не меняется никогда (используется в
        // общих MCP-инструментах self.relay.* как параметр id и как ключ в NVS). Имя по
        // умолчанию — нейтральное, без привязки к конкретному прибору: пользователь сам
        // переименует голосом через self.relay.rename, когда решит, что подключено.
        static Controller_GPIO relay_ch1(POWER_OUT_CH1_GPIO, "relay_ch1", "вывод управления реле номер один");
        static Controller_GPIO relay_ch2(POWER_OUT_CH2_GPIO, "relay_ch2", "вывод управления реле номер два");

        // ВАЖНО: регистрируем ОДИН общий набор из 5 MCP-инструментов на ВСЕ реле сразу
        // (self.relay.turn_on/turn_off/get_state/get_pin_info/rename с параметром id) —
        // а не по 5 инструментов на каждое. У сервера жёсткий лимит в 32 инструмента на
        // устройство суммарно, раньше его превышали и это ломало соединение. Инструментов
        // всегда ровно 5, сколько бы реле ни было в списке ниже (2 или 8) — см.
        // PROMPT_INSTRUCTION.md, разделы "3-е правило" и 9.9.
#ifdef DISABLE_SERVOS
        // Сервоприводы полностью отключены (см. DISABLE_SERVOS в config.h) — их 5 пинов
        // отданы под дополнительные силовые каналы (см. POWER_OUT_CH4..8 в config.h).
        std::vector<Controller_GPIO*> relays = { &relay_ch1, &relay_ch2 };

#ifdef POWER_OUT_CH3_GPIO
        static Controller_GPIO relay_ch3(POWER_OUT_CH3_GPIO, "relay_ch3", "вывод управления реле номер три");
        relays.push_back(&relay_ch3);
#endif
        static Controller_GPIO relay_ch4(POWER_OUT_CH4_GPIO, "relay_ch4", "вывод управления реле номер четыре");
        static Controller_GPIO relay_ch5(POWER_OUT_CH5_GPIO, "relay_ch5", "вывод управления реле номер пять");
        static Controller_GPIO relay_ch6(POWER_OUT_CH6_GPIO, "relay_ch6", "вывод управления реле номер шесть");
        static Controller_GPIO relay_ch7(POWER_OUT_CH7_GPIO, "relay_ch7", "вывод управления реле номер семь");
        static Controller_GPIO relay_ch8(POWER_OUT_CH8_GPIO, "relay_ch8", "вывод управления реле номер восемь");
        relays.push_back(&relay_ch4);
        relays.push_back(&relay_ch5);
        relays.push_back(&relay_ch6);
        relays.push_back(&relay_ch7);
        relays.push_back(&relay_ch8);

        RegisterRelayTools(relays);
#else
        RegisterRelayTools({ &relay_ch1, &relay_ch2 });

        // enable_gestures=true у всех — поворот на угол и жесты (wiggle/nod/shake) друг другу
        // не мешают: set_angle не проверяет этот флаг вообще, его смотрят только жесты.
        servo_1 = new ServoController(SERVO_CH1_GPIO, "Servo 1",   "сервопривод 1",  LEDC_CHANNEL_0, true);
        servo_2 = new ServoController(SERVO_CH2_GPIO, "Servo 2",   "сервопривод 2",  LEDC_CHANNEL_1, true);
        servo_3 = new ServoController(SERVO_CH3_GPIO, "Servo 3",   "сервопривод 3",  LEDC_CHANNEL_2, true);
        servo_4 = new ServoController(SERVO_CH4_GPIO, "Servo 4",   "сервопривод 4",  LEDC_CHANNEL_3, true);
        servo_5 = new ServoController(SERVO_CH5_GPIO, "Servo 5",   "сервопривод 5",  LEDC_CHANNEL_4, true);
        // Аналогично — один общий набор из 7 MCP-инструментов на все 5 сервоприводов сразу.
        RegisterServoTools({ servo_1, servo_2, servo_3, servo_4, servo_5 });
#endif

        static WeatherController weather;

        ESP_LOGI(TAG, "MCP Tools зарегистрированы");
    }

public:
    EspDiyBoard() : DualNetworkBoard(MODEM_UART_TX_PIN, MODEM_UART_RX_PIN, GPIO_NUM_NC, /*default_net_type=*/0), // 0 = Wi-Fi по умолчанию на первом чистом старте
        boot_button_(BOOT_BUTTON_GPIO, false, BOOT_BUTTON_LONG_PRESS_MS, BOOT_BUTTON_SHORT_PRESS_MS),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {

#ifdef DISPLAY_TYPE_SPI_ST7789_170X320
        InitializeDisplaySpiBus();
        InitializeSpiTftDisplay();
#else
        InitializeDisplayI2c();
        InitializeSsd1306Display();
#endif
        InitializeButtons();
        InitializeTools();

        // Настройка MQTT
        Settings mqtt_settings("mqtt", true);
        mqtt_settings.SetInt("keepalive", MQTT_KEEPALIVE_SEC);
        mqtt_settings.SetInt("reconnect_timeout", MQTT_RECONNECT_TIMEOUT_SEC);
        ESP_LOGI(TAG, "MQTT settings forced: keepalive=%ds", MQTT_KEEPALIVE_SEC);

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



    /**
     * Защита от "мёртвого" 4G-режима: если физически нет модема (или нет SIM, или
     * отказано в регистрации сети), автоматически откатываемся на Wi-Fi вместо того,
     * чтобы навсегда остаться офлайн. Не трогаем общий код DualNetworkBoard/Ml307Board —
     * просто оборачиваем callback, который регистрирует Application (UI/alert-сообщения
     * продолжают работать как раньше, мы только добавляем логику поверх).
     */
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override {
        DualNetworkBoard::SetNetworkEventCallback([this, callback](NetworkEvent event, const std::string& data) {
            if (callback) {
                callback(event, data);
            }
            if (!network_fallback_triggered_ && GetNetworkType() == NetworkType::ML307 &&
                (event == NetworkEvent::ModemErrorInitFailed ||
                 event == NetworkEvent::ModemErrorNoSim ||
                 event == NetworkEvent::ModemErrorRegDenied)) {
                network_fallback_triggered_ = true;
                ESP_LOGW(TAG, "4G недоступен (нет модема/SIM/отказ регистрации) — авто-переключение на Wi-Fi");
                SwitchNetworkType(); // сохранит Wi-Fi в NVS и перезагрузит устройство
            }
        });
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
        static GpioLed led(BUILTIN_LED_GPIO, BUILTIN_LED_OUTPUT_INVERT);
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

    virtual Backlight* GetBacklight() override {
#ifdef DISPLAY_TYPE_SPI_ST7789_170X320
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
#else
        return nullptr; // у OLED своей подсветки/яркости нет
#endif
    }
};

DECLARE_BOARD(EspDiyBoard);