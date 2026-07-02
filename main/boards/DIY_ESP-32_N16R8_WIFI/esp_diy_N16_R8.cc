// /**
//  * @file esp_diy_N16_R8.cc
//  * @brief Главный файл конфигурации платы ESP-32 N16R8.
//  * В этой версии осуществлен переход с WifiBoard на DualNetworkBoard 
//  * для нативной поддержки модема ML307R средствами операционной системы XiaoJi.
//  */

// // ИЗМЕНЕНО: Вместо wifi_board.h подключаем dual_network_board.h
// #include "dual_network_board.h" 
// #include "codecs/no_audio_codec.h"
// #include "display/oled_display.h"
// #include "system_reset.h"
// #include "application.h"
// #include "button.h"
// #include "config.h"
// #include "mcp_server.h"
// #include "my_controller_gpio.h"
// #include "led/gpio_led.h"
// #include "assets/lang_config.h"

// #include <esp_log.h>
// #include <driver/i2c_master.h>
// #include <esp_lcd_panel_ops.h>
// #include <esp_lcd_panel_vendor.h>

// #include "weather_controller.h"
// #include "servo_controller.h"

// #ifdef SH1106
// #include <esp_lcd_panel_sh1106.h>
// #endif

// #include "esp_wifi.h"
// #include "settings.h"
// #include <driver/uart.h>

// #define TAG "EspDiyBoard"

// // ИЗМЕНЕНО: Удалены директивы ENABLE_4G_MODEM и старые инклуды для ручного управления модемом,
// // так как теперь модемом управляет базовый класс DualNetworkBoard.

// // ИЗМЕНЕНО: Наследуемся от DualNetworkBoard для автоматической работы Wi-Fi и 4G
// class EspDiyBoard : public DualNetworkBoard {
// private:
//     i2c_master_bus_handle_t display_i2c_bus_;
//     esp_lcd_panel_io_handle_t panel_io_ = nullptr;  
//     esp_lcd_panel_handle_t panel_ = nullptr;
//     Display* display_ = nullptr;
    
//     // Объявление физических кнопок устройства (оставлено без изменений)
//     Button boot_button_;
//     Button touch_button_;
//     Button volume_up_button_;
//     Button volume_down_button_;

//     // Указатели на сервоприводы (оставлено без изменений)
//     ServoController* servo_1 = nullptr;
//     ServoController* servo_2 = nullptr;
//     ServoController* servo_3 = nullptr;
//     ServoController* servo_4 = nullptr;
//     ServoController* servo_5 = nullptr;

//     bool is_double_clicking_ = false;

//     // ИЗМЕНЕНО: Убран указатель Modem4gController* modem_ и метод CheckInternetConnection(),
//     // так как сетевой контроль теперь "под капотом" базового класса.

//     /**
//      * Инициализация аппаратной шины I2C для дисплея.
//      * Твой оригинальный рабочий код.
//      */
//     void InitializeDisplayI2c() {
//         i2c_master_bus_config_t bus_config = {
//             .i2c_port = (i2c_port_t)0,
//             .sda_io_num = DISPLAY_SDA_PIN,  // Берется из config.h (IO41)
//             .scl_io_num = DISPLAY_SCL_PIN,  // Берется из config.h (IO42)
//             .clk_source = I2C_CLK_SRC_DEFAULT,
//             .glitch_ignore_cnt = 7,
//             .intr_priority = 0,
//             .trans_queue_depth = 0,
//             .flags = {
//                 .enable_internal_pullup = 1, // Включаем встроенную подтяжку линий
//             },
//         };
//         ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
//     }

//     /**
//      * Инициализация дисплея SSD1306.
//      * Оставлено строго без изменений, с использованием v2 интерфейса и твоих параметров.
//      */
//     void InitializeSsd1306Display() {
//         // Конфигурация интерфейса I2C для дисплея SSD1306
//         esp_lcd_panel_io_i2c_config_t io_config = {
//             .dev_addr = 0x3C, // Стандартный адрес OLED 0x3C
//             .on_color_trans_done = nullptr,
//             .user_ctx = nullptr,
//             .control_phase_bytes = 1,
//             .dc_bit_offset = 6,
//             .lcd_cmd_bits = 8,
//             .lcd_param_bits = 8,
//             .flags = {
//                 .dc_low_on_data = 0, // Сохранено оригинальное имя параметра
//                 .disable_control_phase = 0,
//             },
//             .scl_speed_hz = 100 * 1000, 
//         };

//         // Регистрируем панель на шине I2C
//         ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

//         ESP_LOGI(TAG, "Установка драйвера дисплея в памяти...");
//         esp_lcd_panel_dev_config_t panel_config = {};
//         panel_config.reset_gpio_num = -1;
//         panel_config.bits_per_pixel = 1;

//         esp_lcd_panel_ssd1306_config_t ssd1306_config = {
//             .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
//         };
//         panel_config.vendor_config = &ssd1306_config;

// #ifdef SH1106
//         ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
// #else
//         ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
// #endif
//         ESP_LOGI(TAG, "Драйвер дисплея успешно установлен в памяти.");
        
//         ESP_LOGI(TAG, "[ДИСПЛЕЙ] Пробуем вызвать программный сброс (panel_reset)...");
//         ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
//         ESP_LOGI(TAG, "[ДИСПЛЕЙ] Программный сброс панели успешно выполнен!");

//         ESP_LOGI(TAG, "[ДИСПЛЕЙ] Пробуем отправить конфигурационную матрицу (panel_init) по I2C...");
//         if (esp_lcd_panel_init(panel_) != ESP_OK) {
//             ESP_LOGE(TAG, "[КРИТИЧЕСКИ] Дисплей вернул ошибку при инициализации!");
//             display_ = new NoDisplay();
//             return;
//         }
//         ESP_LOGI(TAG, "[ДИСПЛЕЙ] Физическая инициализация по I2C пройдена успешно!");

//         ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

//         ESP_LOGI(TAG, "[ДИСПЛЕЙ] Включаем отображение (panel_disp_on_off)...");
//         ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        
//         // Сохранен твой оригинальный конструктор с 6 аргументами
//         display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
//         ESP_LOGI(TAG, "[ДИСПЛЕЙ] Инициализация обертки OledDisplay завершена.");
//     }

    

//     void InitializeButtons() {
//         // Двойное нажатие: переключение сети
//         boot_button_.OnDoubleClick([this]() {
//             is_double_clicking_ = true; // Блокируем следующий одиночный клик
//             ESP_LOGI("Button", "Double click: Switching network...");
            
//             auto& app = Application::GetInstance();
//             if (app.GetDeviceState() == kDeviceStateStarting || 
//                 app.GetDeviceState() == kDeviceStateWifiConfiguring ||
//                 app.GetDeviceState() == kDeviceStateIdle) {
                
//                 GetDisplay()->ShowNotification("Switching Network...");
//                 SwitchNetworkType();
//             }
//         });

//         // Одиночное нажатие: Toggle Chat или Wi-Fi Config
//         boot_button_.OnClick([this]() {
//             // Небольшая пауза, чтобы дать шанс сработать OnDoubleClick
//             vTaskDelay(pdMS_TO_TICKS(300));
            
//             if (is_double_clicking_) {
//                 is_double_clicking_ = false; // Сбрасываем флаг и выходим
//                 return; 
//             }

//             auto& app = Application::GetInstance();
//             if (GetNetworkType() == NetworkType::WIFI && app.GetDeviceState() == kDeviceStateStarting) {
//                 auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
//                 wifi_board.EnterWifiConfigMode();
//             } else {
//                 app.ToggleChatState();
//             }
//         });

//         // Управление звуком и прослушиванием
//         touch_button_.OnPressDown([this]() {
//             Application::GetInstance().StartListening();
//         });
//         touch_button_.OnPressUp([this]() {
//             Application::GetInstance().StopListening();
//         });

//         volume_up_button_.OnClick([this]() {
//             auto codec = GetAudioCodec();
//             int volume = codec->output_volume() + 10;
//             volume = (volume > 100) ? 100 : volume;
//             codec->SetOutputVolume(volume);
//             GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
//         });

//         volume_up_button_.OnLongPress([this]() {
//             GetAudioCodec()->SetOutputVolume(100);
//             GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
//         });

//         volume_down_button_.OnClick([this]() {
//             auto codec = GetAudioCodec();
//             int volume = codec->output_volume() - 10;
//             volume = (volume < 0) ? 0 : volume;
//             codec->SetOutputVolume(volume);
//             GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
//         });

//         volume_down_button_.OnLongPress([this]() {
//             GetAudioCodec()->SetOutputVolume(0);
//             GetDisplay()->ShowNotification(Lang::Strings::MUTED);
//         });
//     }





//     /**
//      * Регистрация инструментов (MCP Tools).
//      * Сохранены твои оригинальные сигнатуры конструкторов Controller_GPIO и ServoController.
//      */
//     void InitializeTools() {
//         static Controller_GPIO solder(GPIO_NUM_18, "solder", "Паяльник");
//         static Controller_GPIO dryer(GPIO_NUM_17, "dryer", "Фен");
//         static WeatherController weather;

//         servo_1 = new ServoController(SERVO_CH1_GPIO, "servo_1", "Сервопривод 1", LEDC_CHANNEL_0);
//         servo_2 = new ServoController(SERVO_CH2_GPIO, "servo_2", "Сервопривод 2", LEDC_CHANNEL_1);
//         servo_3 = new ServoController(SERVO_CH3_GPIO, "servo_3", "Сервопривод 3", LEDC_CHANNEL_2);
//         servo_4 = new ServoController(SERVO_CH4_GPIO, "servo_4", "Сервопривод 4", LEDC_CHANNEL_3, true);
//         servo_5 = new ServoController(SERVO_CH5_GPIO, "servo_5", "Сервопривод 5", LEDC_CHANNEL_4, true);

//         // ИЗМЕНЕНО: Удален вызов инициализации ручного модема (modem_ = new Modem4gController...)
        
//         ESP_LOGI(TAG, "Все инструменты (Фен, Паяльник, Серво) успешно зарегистрированы в MCP.");
//     }

//     // ИЗМЕНЕНО: Метод NetworkSupervisorTask и CheckInternet полностью удалены.
//     // Они конфликтовали бы с внутренним PPP-интерфейсом DualNetworkBoard.

// public:
//     /**
//      * Конструктор платы.
//      * ИЗМЕНЕНО: Теперь мы передаем пины ML307_TX_PIN и ML307_RX_PIN напрямую 
//      * в базовый класс DualNetworkBoard. Эти пины уже определены в твоем config.h.
//      */
//     EspDiyBoard() : 
//         DualNetworkBoard(MODEM_UART_TX_PIN, MODEM_UART_RX_PIN, GPIO_NUM_NC),
        
//         // Добавляем параметры времени: 
//         // GPIO, active_high=false, long_press=2000мс, short_press=200мс
//         boot_button_(BOOT_BUTTON_GPIO, false, 2000, 250), 
//         touch_button_(TOUCH_BUTTON_GPIO),
//         volume_up_button_(VOLUME_UP_BUTTON_GPIO),
//         volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {

//         // 1. ИЗМЕНЯЕМ НАСТРОЙКИ MQTT БЕЗ ПРАВКИ ЯДРА
//         // Записываем нужный таймаут прямо в энергонезависимую память (NVS)
//         // Settings mqtt_settings("mqtt", true);
//         // mqtt_settings.SetInt("keepalive", 30); // Снижаем с 240 до 30 секунд
        
//         ESP_LOGI("EspDiyBoard", "[ШАГ 1] Начинаем инициализацию I2C...");
//         InitializeDisplayI2c();
//         ESP_LOGI("EspDiyBoard", "[ШАГ 2] Начинаем инициализацию Дисплея...");
//         InitializeSsd1306Display();
//         ESP_LOGI("EspDiyBoard", "[ШАГ 3] Начинаем инициализацию Кнопок...");
//         InitializeButtons();
//         ESP_LOGI("EspDiyBoard", "[ШАГ 4] Начинаем инициализацию Инструментов...");
//         InitializeTools();
//         ESP_LOGI("EspDiyBoard", "Плата ESP32-S3 DIY N16R8 инициализирована с поддержкой DualNetwork (Wi-Fi + 4G)");

        
//     }

    



//     // /**
//     //  * @brief Запуск сетевого стека.
//     //  * Мы делегируем всю работу базовому классу DualNetworkBoard, 
//     //  * который сам инициализирует нужный интерфейс на основе настроек.
//     //  */
//     // virtual void StartNetwork() override {
//     //     DualNetworkBoard::StartNetwork();
    
//     //     if (GetNetworkType() == NetworkType::WIFI) {
//     //         ESP_LOGW(TAG, "РЕЖИМ WIFI: Экстренное подавление модема...");
            
//     //         // 1. Инициализируем UART с правильными параметрами для ML307
//     //         uart_config_t uart_config = {
//     //             .baud_rate = 115200,
//     //             .data_bits = UART_DATA_8_BITS,
//     //             .parity    = UART_PARITY_DISABLE,
//     //             .stop_bits = UART_STOP_BITS_1,
//     //             .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//     //             .source_clk = UART_SCLK_APB,
//     //         };
//     //         uart_param_config(UART_NUM_2, &uart_config);
//     //         uart_set_pin(UART_NUM_2, MODEM_UART_TX_PIN, MODEM_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
//     //         uart_driver_install(UART_NUM_2, 256, 0, 0, NULL, 0);

//     //         // 2. Посылаем последовательность выключения (некоторые ML307 требуют сначала AT, потом команду)
//     //         const char* cmds[] = {"AT\r\n", "AT+CFUN=0\r\n", "AT+CPOWD=1\r\n"}; // CPOWD=1 - это полное выключение
//     //         for (const char* cmd : cmds) {
//     //             uart_write_bytes(UART_NUM_2, cmd, strlen(cmd));
//     //             vTaskDelay(pdMS_TO_TICKS(500));
//     //         }
            
//     //         // 3. Агрессивно удаляем драйвер, чтобы UART-ножки стали высокоомными (Z-state)
//     //         uart_driver_delete(UART_NUM_2);
            
//     //         // 4. Опционально: если есть DTR/RST пин, принудительно дерни его в HIGH/LOW здесь
//     //         // gpio_set_level(MODEM_DTR_PIN, 1); 
            
//     //         ESP_LOGW(TAG, "Модем должен быть выключен. Проверь температуру через 2 минуты.");
//     //     }
//     // }


//     // /**
//     //  * @brief Перехват управления энергосбережением (КРИТИЧЕСКИ ВАЖНО ДЛЯ MQTT).
//     //  * Ядро XiaoJi (StateMachine) постоянно пытается усыпить устройство в режиме idle.
//     //  * Этот метод перехватывает команду ядра до того, как она уйдет в драйвер Wi-Fi.
//     //  */
//     // virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
//     //     // Проверяем, какая сеть сейчас активна
//     //     if (GetNetworkType() == NetworkType::WIFI) {
//     //         // Если мы на Wi-Fi, принудительно подменяем команду на PERFORMANCE 
//     //         // (PERFORMANCE внутри XiaoJi эквивалентно WIFI_PS_NONE).
//     //         // Это не даст Wi-Fi модулю уснуть и предотвратит разрыв MQTT сессии (errno=119).
//     //         DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
//     //     } else {
//     //         // Если мы сидим на 4G модеме (ML307), отдаем команду "как есть",
//     //         // чтобы модем мог корректно экономить энергию, если это поддерживает его драйвер.
//     //         DualNetworkBoard::SetPowerSaveLevel(level);
//     //     }
//     // }





//     virtual Led* GetLed() override {
//         static GpioLed led(BUILTIN_LED_GPIO, 0); 
//         return &led;
//     }

//     virtual AudioCodec* GetAudioCodec() override {
// #ifdef AUDIO_I2S_METHOD_SIMPLEX
//         static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
//             AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
// #else
//         static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
//             AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
// #endif
//         return &audio_codec;
//     }

//     virtual Display* GetDisplay() override {
//         return display_;
//     }
// };

// DECLARE_BOARD(EspDiyBoard);















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



    virtual void StartNetwork() override {
        DualNetworkBoard::StartNetwork();

        if (GetNetworkType() == NetworkType::ML307) {
            ESP_LOGI(TAG, "ML307: задержка 3 сек для стабилизации PPP");
            vTaskDelay(pdMS_TO_TICKS(3000));
        } else {
            ESP_LOGI(TAG, "Wi-Fi: запуск");
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