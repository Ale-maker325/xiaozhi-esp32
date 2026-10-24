#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// =============================================================================
// 1. АУДИО ПЕРИФЕРИЯ (Жесткий стандарт Xiaozhi - I2S Simplex)
// =============================================================================
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_METHOD_SIMPLEX    // Микрофон и динамик работают на раздельных шинах клоков

// Входной тракт: Цифровой микрофон INMP441
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// Выходной тракт: Усилитель класса D MAX98357
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

// Кнопки громкости (esp_diy_N16_R8.cc::InitializeButtons())
#define VOLUME_STEP  10   // На сколько % меняем громкость за один клик
#define VOLUME_MAX   100  // Потолок громкости (%)


// =============================================================================
// 2. СВЯЗЬ И МОДЕМ (Официальная разводка под сотовый модем ML307R 4G Cat1)
// =============================================================================
// Плата всегда двухрежимная (Wi-Fi + 4G) — переключение между ними происходит в
// рантайме (двойной клик BOOT), а не выбором на этапе компиляции. Модем при этом
// НЕ обязателен физически: если его нет (или нет SIM, или отказано в регистрации),
// прошивка сама это обнаружит и автоматически откатится на Wi-Fi — см.
// EspDiyBoard::SetNetworkEventCallback в esp_diy_N16_R8.cc.
#define MODEM_UART_TX_PIN       GPIO_NUM_12  // Линия передачи ESP32 -> RX модема
#define MODEM_UART_RX_PIN       GPIO_NUM_11  // Линия приема ESP32 <- TX модема
#define MODEM_PWR_PIN           GPIO_NUM_13  // Управление включением (PWRKEY модема, пока не задействован)


// =============================================================================
// 3. СИСТЕМНЫЕ КНОПКИ И ПЕРИФЕРИЯ МИКРОКОНТРОЛЛЕРА
// =============================================================================
#define BOOT_BUTTON_GPIO        GPIO_NUM_0   // Физическая кнопка BOOT на плате (режим прошивки)
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40  // Кнопка увеличения громкости
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39  // Кнопка уменьшения громкости
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47  // Сенсорная / функциональная кнопка управления

// Тайминги кнопки BOOT (esp_diy_N16_R8.cc, конструктор EspDiyBoard — boot_button_(...))
#define BOOT_BUTTON_LONG_PRESS_MS  2000  // Долгое удержание (мс)
#define BOOT_BUTTON_SHORT_PRESS_MS 250   // Порог короткого клика (мс)

// ВНИМАНИЕ: На твоей плате пин 48 жестко разведен на встроенный светодиод!
// Он работает на уровне логики 1.8В (особенность чипов с Octal PSRAM). Резервируем под системный LED.
#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BUILTIN_LED_OUTPUT_INVERT 0  // 0 = обычная полярность, 1 = инвертировать сигнал


// =============================================================================
// 4. ДИСПЛЕЙ — по умолчанию встроенный I2C OLED (SSD1306/SH1106).
//    Альтернатива: отдельный SPI TFT-модуль ST7789 1.9" 170x320
//    (пины модуля: GND, VCC, SCL->SCK, SDA->MOSI, RES, DC, CS, BLK).
// =============================================================================

// Раскомментируйте эту строку для сборки с отдельным SPI TFT-модулем ST7789 170x320
// вместо OLED. Физически в этом случае OLED (I2C, GPIO41/42) не подключайте.
// #define DISPLAY_TYPE_SPI_ST7789_170X320

#ifdef DISPLAY_TYPE_SPI_ST7789_170X320
    // --- Вариант: SPI TFT ST7789 170x320 ---
    // Калибровочные параметры (offset/invert/rgb order) взяты из рабочего референса
    // main/boards/bread-compact-wifi-lcd (та же самая матрица, там уже подтверждённо
    // работает без искажений отрисовки).
    #define DISPLAY_SPI_SCLK_PIN     GPIO_NUM_21
    #define DISPLAY_SPI_MOSI_PIN     GPIO_NUM_38
    #define DISPLAY_SPI_CS_PIN       GPIO_NUM_14
    #define DISPLAY_SPI_DC_PIN       GPIO_NUM_45  // strapping-пин, но дёргается редко (только между командой/данными)
    #define DISPLAY_SPI_RST_PIN      GPIO_NUM_46  // strapping-пин, но дёргается один раз при инициализации
    #define DISPLAY_BACKLIGHT_PIN    GPIO_NUM_18  // переиспользован из POWER_OUT_CH3 (был свободен) — см. раздел 5

    #define DISPLAY_WIDTH            170
    #define DISPLAY_HEIGHT           320
    #define DISPLAY_MIRROR_X         false
    #define DISPLAY_MIRROR_Y         false
    #define DISPLAY_SWAP_XY          false
    #define DISPLAY_INVERT_COLOR     true
    #define DISPLAY_RGB_ORDER        LCD_RGB_ELEMENT_ORDER_RGB
    #define DISPLAY_OFFSET_X         35  // матрица физически 170 точек, память контроллера — 172, сдвиг обязателен
    #define DISPLAY_OFFSET_Y         0
    #define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
    #define DISPLAY_SPI_MODE         0
    #define DISPLAY_SPI_SCLK_HZ      (40 * 1000 * 1000)
#else
    // --- Вариант по умолчанию: I2C OLED (как было) ---
    #define DISPLAY_SDA_PIN         GPIO_NUM_41
    #define DISPLAY_SCL_PIN         GPIO_NUM_42
    #define DISPLAY_I2C_ADDR        0x3C          // стандартный адрес SSD1306/SH1106
    #define DISPLAY_I2C_FREQ_HZ     (400 * 1000)  // 400 кГц — Fast-mode I2C
    #define DISPLAY_WIDTH           128

    #if CONFIG_OLED_SSD1306_128X32
    #define DISPLAY_HEIGHT          32
    #elif CONFIG_OLED_SSD1306_128X64
    #define DISPLAY_HEIGHT          64
    #elif CONFIG_OLED_SH1106_128X64
    #define DISPLAY_HEIGHT          64
    #define SH1106
    #else
    #error "OLED display type is not selected"
    #endif

    #define DISPLAY_MIRROR_X        true
    #define DISPLAY_MIRROR_Y        true
#endif


// =============================================================================
// 5. КАСТОМНАЯ ПЕРИФЕРИЯ МАКЕТНОЙ ПЛАТЫ (Выбрана из свободных доступных пинов)
// =============================================================================

// Раскомментируйте, чтобы ПОЛНОСТЬЮ отключить поддержку сервоприводов на этой сборке
// платы (не всем нужны серво — иногда физически нужны только реле). Освобождает все 5
// пинов серво (GPIO1,2,3,8,9) под ДОПОЛНИТЕЛЬНЫЕ силовые каналы (реле) — см. ниже,
// POWER_OUT_CH4..8. Класс ServoController и его 7 MCP-инструментов (self.servo.*) при
// этом просто не создаются — бюджет MCP-инструментов не растёт, а освобождается (см.
// PROMPT_INSTRUCTION.md, раздел 9.9).
// #define DISABLE_SERVOS

// Раскомментируйте, чтобы сервоприводы управлялись через внешний 16-канальный I2C
// ШИМ-контроллер PCA9685 вместо прямых GPIO/LEDC ESP32-S3 (требует физически распаянный
// модуль PCA9685). Освобождает 5 пинов (GPIO1,2,3,8,9) — см. раздел 5а ниже. Смысла вместе
// с DISABLE_SERVOS не имеет (см. #error ниже) — серво всё равно не будет.
// #define PCA9685_OUTPUT

#if defined(DISABLE_SERVOS) && defined(PCA9685_OUTPUT)
#error "DISABLE_SERVOS и PCA9685_OUTPUT одновременно не имеют смысла: при отключенных сервоприводах настраивать для них I2C-бэкенд незачем (config.h, раздел 5)"
#endif

// --- СЕРВОПРИВОДЫ: общая настройка (одинакова для обоих бэкендов — LEDC и PCA9685; при
//     DISABLE_SERVOS не используется вовсе, серво в сборке просто нет) ---
#ifndef DISABLE_SERVOS
#define SERVO_MIN_PULSEWIDTH_US (500)  // Импульс для положения 0 градусов (мкс)
#define SERVO_MAX_PULSEWIDTH_US (2500) // Импульс для положения 180 градусов (мкс)
#define SERVO_MAX_DEGREE        (180)  // Аппаратный лимит угла поворота сервопривода
#define SERVO_FREQ_HZ           (50)   // Частота управляющего ШИМ-сигнала (50 Гц, стандарт)

// --- СЕРВОПРИВОДЫ: пины/каналы ---
#ifdef PCA9685_OUTPUT
    // В режиме PCA9685 значения этих макросов — НЕ номера GPIO, а номера каналов чипа
    // PCA9685 (0-15). Переиспользуем те же имена и тип gpio_num_t как простую "коробку"
    // для целого числа — так конструкторы ServoController(...) в esp_diy_N16_R8.cc можно
    // оставить вообще без изменений, независимо от режима.
    #define SERVO_CH1_GPIO   ((gpio_num_t)0)
    #define SERVO_CH2_GPIO   ((gpio_num_t)1)
    #define SERVO_CH3_GPIO   ((gpio_num_t)2)
    #define SERVO_CH4_GPIO   ((gpio_num_t)3)
    #define SERVO_CH5_GPIO   ((gpio_num_t)4)
#else
    // --- Вариант по умолчанию: 5 каналов ШИМ на базе аппаратного модуля LEDC ---
    #define SERVO_CH1_GPIO   GPIO_NUM_1
    #define SERVO_CH2_GPIO   GPIO_NUM_2
    #define SERVO_CH3_GPIO   GPIO_NUM_3
    #define SERVO_CH4_GPIO   GPIO_NUM_8
    #define SERVO_CH5_GPIO   GPIO_NUM_9
#endif
#endif // DISABLE_SERVOS

// --- СИЛОВЫЕ КЛЮЧИ (Управление MOSFET / Реле для ЛЮБОГО внешнего оборудования —
//     фен/паяльник/т.п., см. InitializeTools() — это НЕ питание модема, для того
//     есть отдельный MODEM_PWR_PIN в разделе 2) ---
#define POWER_OUT_CH1_GPIO  GPIO_NUM_10
#define POWER_OUT_CH2_GPIO  GPIO_NUM_17
// GPIO18 (3-й силовой канал) занят подсветкой SPI-дисплея, если включён
// DISPLAY_TYPE_SPI_ST7789_170X320 (см. раздел 4) — оба одновременно не использовать.
// Подключается в InitializeTools() только если доступен (см. #ifdef POWER_OUT_CH3_GPIO
// в esp_diy_N16_R8.cc) — т.е. только в режиме DISABLE_SERVOS ниже; при обычном режиме
// (серво включены) остаётся зарезервированным на будущее, как и раньше.
#ifndef DISPLAY_TYPE_SPI_ST7789_170X320
#define POWER_OUT_CH3_GPIO  GPIO_NUM_18
#endif

#ifdef DISABLE_SERVOS
// --- ДОПОЛНИТЕЛЬНЫЕ силовые каналы — доступны, только если сервоприводы полностью
//     отключены (см. DISABLE_SERVOS выше). Физически это те же самые 5 ножек платы, что
//     были бы заняты под SERVO_CH1..5_GPIO — просто к ним в этом случае подключается
//     силовой ключ/реле вместо сервопривода. Итого силовых каналов на плате: CH1,2
//     (всегда) + CH3 (если не занят SPI-дисплеем) + CH4..8 (эти) = до 8 штук.
#define POWER_OUT_CH4_GPIO  GPIO_NUM_1   // был SERVO_CH1_GPIO
#define POWER_OUT_CH5_GPIO  GPIO_NUM_2   // был SERVO_CH2_GPIO
// GPIO3 — strapping-пин (выбор режима загрузки), см. раздел 3 PROMPT_INSTRUCTION.md: то
// же самое предупреждение, что было для SERVO_CH3_GPIO, актуально и здесь.
#define POWER_OUT_CH6_GPIO  GPIO_NUM_3   // был SERVO_CH3_GPIO
#define POWER_OUT_CH7_GPIO  GPIO_NUM_8   // был SERVO_CH4_GPIO
#define POWER_OUT_CH8_GPIO  GPIO_NUM_9   // был SERVO_CH5_GPIO
#endif

#ifdef PCA9685_OUTPUT
// --- 5а. I2C-ШИНА ДЛЯ PCA9685 ---
// Отдельная, полностью независимая от шины OLED-дисплея (GPIO41/42) шина — специально,
// чтобы сервоприводы никак не могли повлиять на экран (и наоборот). Сидит на пинах,
// освободившихся от прямого управления серво (см. выше).
#define PCA9685_I2C_NUM       I2C_NUM_1
#define PCA9685_I2C_SDA_PIN   GPIO_NUM_1   // был SERVO_CH1_GPIO
#define PCA9685_I2C_SCL_PIN   GPIO_NUM_2   // был SERVO_CH2_GPIO
#define PCA9685_I2C_ADDR      0x40         // адрес по умолчанию (без перемычек на плате PCA9685)
#define PCA9685_I2C_FREQ_HZ   (400 * 1000) // 400 кГц — стандартный Fast-mode I2C
// GPIO3, 8, 9 (бывшие SERVO_CH3/4/5) остаются полностью свободны в этом режиме.
#endif


// =============================================================================
// 6. СПРАВОЧНИК СВОБОДНЫХ РЕСУРСОВ (Карта пинов на будущее)
// =============================================================================
/* * Ниже перечислены пины, которые физически выведены на твоей плате и свободны,
 * если НЕ включён DISPLAY_TYPE_SPI_ST7789_170X320 (раздел 4) — в этом режиме пины
 * 21, 38, 45, 46, 14 и 18 уже заняты SPI-дисплеем/его подсветкой (см. раздел 4 и 5):
 * * GPIO_NUM_21  - Полностью свободный GPIO общего назначения (занят под DISPLAY_SPI_SCLK, если включён SPI-дисплей).
 * GPIO_NUM_38  - Свободный GPIO (альтернативная функция JTAG MTCK; занят под DISPLAY_SPI_MOSI, если включён SPI-дисплей).
 * GPIO_NUM_45  - Свободный GPIO (Внимание: Strapping pin питания VDD_SPI, использовать аккуратно; занят под DISPLAY_SPI_DC, если включён SPI-дисплей).
 * GPIO_NUM_46  - Свободный GPIO (Внимание: Strapping pin JTAG/Логирования, при старте должен быть притянут к GND; занят под DISPLAY_SPI_RST, если включён SPI-дисплей).
 * GPIO_NUM_14  - Свободный GPIO (можно использовать для внешнего датчика или кнопки; занят под DISPLAY_SPI_CS, если включён SPI-дисплей).
 */


// =============================================================================
// 7. ПРОЧИЕ НАСТРОЙКИ ПОВЕДЕНИЯ (не пины, но тоже то, что может захотеться поменять)
// =============================================================================

// --- MQTT (esp_diy_N16_R8.cc, конструктор — принудительно пишет это в NVS при каждом старте) ---
#define MQTT_KEEPALIVE_SEC          60  // Интервал keepalive-пингов
#define MQTT_RECONNECT_TIMEOUT_SEC  8   // Таймаут переподключения

// --- Погода (weather_controller.h) — город по умолчанию, если ИИ не смог определить
//     местоположение пользователя по IP ---
#define DEFAULT_CITY "Каменское, Днепропетровская обл., Украина"

#endif // _BOARD_CONFIG_H_