#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"

//#include "led/single_led.h"
// 1. Меняем инклуд в начале файла
#include "led/gpio_led.h"

#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "weather_controller.h"
#include "servo_controller.h"
#include "meshtastic_controller.h"

#include "lora_custom_controller.h"


#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "EspDiyBoard"

class EspDiyBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    ServoController* servo_head_ = nullptr;
    ServoController* servo_hand_ = nullptr;// Добавляем указатель на сервопривод в класс платы потому что в функции InitializeTools вы уже используете servo_ = new ServoController(...), но сам компилятор не знает, что это за переменная.




    
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,  // Теперь берет IO41 из config.h
            .scl_io_num = DISPLAY_SCL_PIN, // Теперь берет IO42 из config.h
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
        // Используем параметры зеркалирования из config.h
        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        // Кнопка BOOT (IO0) - активация чата или сброс Wi-Fi
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        // Кнопка Volume+ (IO40)
        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        // Кнопка Volume- (IO39)
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }


    // Инициализация инструментов (Tools) для взаимодействия с Джарвисом через MCP
    // Каждый инструмент - это команда, которую Джарвис может вызвать по голосовой команде или из облака
    // В данном случае мы создаем инструменты для управления лампами через MCP протокол (Мастер Контроль Платформа)
    // Каждый инструмент регистрируется в McpServer с уникальным именем, описанием и функцией-обработчиком
    // Функция-обработчик вызывается, когда Джарвис вызывает этот инструмент через MCP протокол (например, по голосовой команде) 
    // и возвращает результат выполнения команды обратно Джарвису (например, текст для озвучивания) 
    // Таким образом, мы расширяем функциональность платы, позволяя Джарвису управлять внешними устройствами (лампами) через голосовые команды или облачные сценарии
    // MCP (Model Context Protocol) — это «язык общения» между искусственным интеллектом (моделью) и твоим устройством.
    // Джарвис использует MCP, чтобы отправлять команды твоему устройству и получать от него данные. 
    // Инструменты (Tools) — это «набор команд», которые твое устройство может выполнять по запросу Джарвиса через MCP.
    void InitializeTools() {
        // Создаем объекты статически, чтобы они жили всё время работы программы
        // Первый параметр — ID для системы (латиницей), второй — имя для Джарвиса (на русском)
        static LampController solder(GPIO_NUM_18, "solder", "Паяльник");
        static LampController dryer(GPIO_NUM_17, "dryer", "Фен");

        // Создаем объект погоды один раз
        static WeatherController weather;

        // Инициализация сервопривода. Пин GPIO_NUM_XXX.
        // Объект создается один раз и сам регистрирует свою команду в MCP
        // Явно создаем объект на GPIO
        // servo_ = new ServoController(GPIO_NUM_2, LEDC_CHANNEL_2);
        
        // Пин 2 для головы, Канал 2
        servo_head_ = new ServoController(GPIO_NUM_2, "head", LEDC_CHANNEL_2);
        // Пин 4 для руки (замени на свой), Канал 3 (обязательно другой канал!)
        servo_hand_ = new ServoController(GPIO_NUM_1, "hand", LEDC_CHANNEL_3);

        // НОВЫЙ КОНТРОЛЛЕР: Meshtastic (используем пины 10 и 11)
        static MeshtasticController lora(GPIO_NUM_10, GPIO_NUM_11);

        static LoraCustomController lora_module;
        
        ESP_LOGI(TAG, "Все инструменты (Фен, Паяльник, Серво, Meshtastic) готовы.");
        
    }






public:
    EspDiyBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();
        ESP_LOGI("EspDiyBoard", "Плата ESP32-S3 DIY N16R8 инициализирована");
    }



    // virtual Led* GetLed() override {
    //     static SingleLed led(BUILTIN_LED_GPIO);
    //     return &led;
    // }
    // 2. Переписываем метод GetLed
    virtual Led* GetLed() override {
        // Используем GpioLed вместо SingleLed
        // BUILTIN_LED_GPIO у нас 48. 
        // Второй параметр '0' — это инверсия (0 - прямой сигнал, 1 - инверсный).
        static GpioLed led(BUILTIN_LED_GPIO, 0); 
        return &led;
    }



    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(EspDiyBoard);
