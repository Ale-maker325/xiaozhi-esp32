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


// =============================================================================
// 2. СВЯЗЬ И МОДЕМ (Официальная разводка под сотовый модем ML307R 4G Cat1)
// =============================================================================
#define MODEM_ON // Включить поддержку модема ML307R (4G Cat1) на базе AT-команд (раскомментировать, если модем есть на плате)

#if defined(MODEM_ON)
    #define MODEM_UART_TX_PIN       GPIO_NUM_12  // Линия передачи ESP32 -> RX модема
    #define MODEM_UART_RX_PIN       GPIO_NUM_11  // Линия приема ESP32 <- TX модема
    #define MODEM_PWR_PIN           GPIO_NUM_13  // Управление включением (PWRKEY модема)
#endif


// =============================================================================
// 3. СИСТЕМНЫЕ КНОПКИ И ПЕРИФЕРИЯ МИКРОКОНТРОЛЛЕРА
// =============================================================================
#define BOOT_BUTTON_GPIO        GPIO_NUM_0   // Физическая кнопка BOOT на плате (режим прошивки)
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40  // Кнопка увеличения громкости
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39  // Кнопка уменьшения громкости
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47  // Сенсорная / функциональная кнопка управления

// ВНИМАНИЕ: На твоей плате пин 48 жестко разведен на встроенный светодиод!
// Он работает на уровне логики 1.8В (особенность чипов с Octal PSRAM). Резервируем под системный LED.
#define BUILTIN_LED_GPIO        GPIO_NUM_48  


// =============================================================================
// 4. ДИСПЛЕЙ (Интерфейс I2C для OLED SSD1306 / SH1106)
// =============================================================================
#define DISPLAY_SDA_PIN         GPIO_NUM_41
#define DISPLAY_SCL_PIN         GPIO_NUM_42
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


// =============================================================================
// 5. КАСТОМНАЯ ПЕРИФЕРИЯ МАКЕТНОЙ ПЛАТЫ (Выбрана из свободных доступных пинов)
// =============================================================================

// --- СЕРВОПРИВОДЫ (5 каналов ШИМ на базе аппаратного модуля LEDC) ---
#define SERVO_CH1_GPIO   GPIO_NUM_1
#define SERVO_CH2_GPIO   GPIO_NUM_2
#define SERVO_CH3_GPIO   GPIO_NUM_3
#define SERVO_CH4_GPIO   GPIO_NUM_8
#define SERVO_CH5_GPIO   GPIO_NUM_9

// --- СИЛОВЫЕ КЛЮЧИ (Управление MOSFET / Реле) ---
#define POWER_OUT_CH1_GPIO  GPIO_NUM_10
#define POWER_OUT_CH2_GPIO  GPIO_NUM_17
#define POWER_OUT_CH3_GPIO  GPIO_NUM_18


// =============================================================================
// 6. СПРАВОЧНИК СВОБОДНЫХ РЕСУРСОВ (Карта пинов на будущее)
// =============================================================================
/* * Ниже перечислены пины, которые физически выведены на твоей плате, 
 * полностью свободны в текущей конфигурации и могут быть использованы позже 
 * (например, под будущую секцию LoRa, SPI-дисплей или внешние датчики):
 * * GPIO_NUM_21  - Полностью свободный GPIO общего назначения.
 * GPIO_NUM_38  - Свободный GPIO (альтернативная функция JTAG MTCK).
 * GPIO_NUM_45  - Свободный GPIO (Внимание: Strapping pin питания VDD_SPI, использовать аккуратно).
 * GPIO_NUM_46  - Свободный GPIO (Внимание: Strapping pin JTAG/Логирования, при старте должен быть притянут к GND).
 * GPIO_NUM_14  - Свободный GPIO (можно использовать для внешнего датчика или кнопки).
 */

#endif // _BOARD_CONFIG_H_