#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_METHOD_SIMPLEX    //Simplex — это когда микрофон и динамик работают на разных шинах (разные клоки).

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#elif CONFIG_OLED_SH1106_128X64
#define DISPLAY_HEIGHT  64
#define SH1106
#else
#error "OLED display type is not selected"
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

// Пин для сервопривода (выбран GPIO 1, как в твоем коде)
//#define SERVO_PWM_GPIO          GPIO_NUM_1
// // A MCP Test: Control a lamp
// #define LAMP_GPIO GPIO_NUM_18
// // Добавляем вторую
// #define SECOND_LAMP_GPIO GPIO_NUM_17

// --- СЕКЦИЯ LORA (HT-Ra62 / SX1262) ---
#define LORA_SCK_PIN   GPIO_NUM_9
#define LORA_MISO_PIN  GPIO_NUM_11
#define LORA_MOSI_PIN  GPIO_NUM_10
#define LORA_CS_PIN    GPIO_NUM_8
#define LORA_RST_PIN   GPIO_NUM_12
#define LORA_BUSY_PIN  GPIO_NUM_13
#define LORA_DIO1_PIN  GPIO_NUM_14

// Настройки конкретно для HT-Ra62
#define LORA_TCXO_VOLTAGE 1.8f  // Напряжение питания кварца (стандарт для Ra62)
#define LORA_FREQ         433.0 // Частота в МГц

#endif // _BOARD_CONFIG_H_
