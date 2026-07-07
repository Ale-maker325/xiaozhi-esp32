#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// =============================================================================
// 1. АУДИО (I2S Simplex)
// =============================================================================
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_METHOD_SIMPLEX

// Микрофон INMP441
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// Динамик MAX98357
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

// =============================================================================
// 2. МОДЕМ ML307-R
// =============================================================================
#define MODEM_ON

#if defined(MODEM_ON)
    #define MODEM_UART_TX_PIN       GPIO_NUM_12
    #define MODEM_UART_RX_PIN       GPIO_NUM_11
    #define MODEM_PWR_PIN           GPIO_NUM_13     // для MOSFET
#endif

// =============================================================================
// 3. КНОПКИ И LED
// =============================================================================
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define BUILTIN_LED_GPIO        GPIO_NUM_48

// =============================================================================
// 4. ДИСПЛЕЙ ST7789 1.9" (SPI)
// =============================================================================
#define DISPLAY_ST7789_1_9

#define DISPLAY_SDA_PIN         GPIO_NUM_13   // MOSI
#define DISPLAY_SCL_PIN         GPIO_NUM_12   // SCLK
#define DISPLAY_DC_PIN          GPIO_NUM_11
#define DISPLAY_CS_PIN          GPIO_NUM_10
#define DISPLAY_RESET_PIN         GPIO_NUM_1
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_14

#define DISPLAY_SPI_HOST        SPI3_HOST   // ← важно!
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          170

#define DISPLAY_OFFSET_X        0      // ← важно!
#define DISPLAY_OFFSET_Y        40
#define DISPLAY_SWAP_XY         true
#define DISPLAY_MIRROR_X        true
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_BGR


// =============================================================================
// 5. КАСТОМНАЯ ПЕРИФЕРИЯ
// =============================================================================

// Серво
#define SERVO_CH1_GPIO   GPIO_NUM_2
#define SERVO_CH2_GPIO   GPIO_NUM_3
#define SERVO_CH3_GPIO   GPIO_NUM_8
#define SERVO_CH4_GPIO   GPIO_NUM_9
#define SERVO_CH5_GPIO   GPIO_NUM_21

// Силовые ключи (реле/MOSFET)
#define POWER_OUT_CH1_GPIO  GPIO_NUM_17
#define POWER_OUT_CH2_GPIO  GPIO_NUM_18
#define POWER_OUT_CH3_GPIO  GPIO_NUM_38

// =============================================================================
// 6. СВОБОДНЫЕ ПИНЫ (для будущего использования)
// =============================================================================
/*
Свободные пины (выводы на гребенках):
- Левая сторона: GPIO4,5,6,7,15,16,17,18,19,20
- Правая сторона: GPIO2,42,41,40,39,38,45,48,47,21

Уже заняты:
- AUDIO: 4,5,6,7,15,16
- MODEM: 11,12,13
- DISPLAY: 1,10,11,12,13,14
- Кнопки/LED: 0,39,40,47,48
- Серво: 2,3,8,9,21
- Power Out: 17,18,38

Оставшиеся свободные:
- GPIO19, GPIO20, GPIO42, GPIO41, GPIO45 (с осторожностью — strapping)
*/

#endif // _BOARD_CONFIG_H_