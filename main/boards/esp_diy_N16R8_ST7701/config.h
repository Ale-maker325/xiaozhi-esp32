#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// --- АУДИО (Внешние модули INMP441 и MAX98357A) ---
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_WS   GPIO_NUM_16   // WS
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_15   // BCLK
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7    // DIN (Микрофон)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_44   // DOUT (Динамик)

// Пины для I2C (Расширитель и возможные кодеки)
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_47 
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_48

// --- ДИСПЛЕЙ ---
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  480
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_6 

// Пины для инициализации ST7701S по 3-wire SPI
#define LCD_SPI_SDA      GPIO_NUM_1
#define LCD_SPI_SCL      GPIO_NUM_2
#define LCD_SPI_CS       GPIO_NUM_42

// Управляющие сигналы RGB
#define BSP_LCD_PCLK    GPIO_NUM_41
#define BSP_LCD_VSYNC   GPIO_NUM_39
#define BSP_LCD_HSYNC   GPIO_NUM_38
#define BSP_LCD_DE      GPIO_NUM_40

// Пины данных RGB565
#define BSP_LCD_DATA0   GPIO_NUM_5   // B0
#define BSP_LCD_DATA1   GPIO_NUM_45  // B1
#define BSP_LCD_DATA2   GPIO_NUM_46  // B2
#define BSP_LCD_DATA3   GPIO_NUM_3   // B3
#define BSP_LCD_DATA4   GPIO_NUM_8   // B4
#define BSP_LCD_DATA5   GPIO_NUM_18  // G0
#define BSP_LCD_DATA6   GPIO_NUM_17  // G1
#define BSP_LCD_DATA7   GPIO_NUM_9   // G2
#define BSP_LCD_DATA8   GPIO_NUM_10  // G3
#define BSP_LCD_DATA9   GPIO_NUM_11  // G4
#define BSP_LCD_DATA10  GPIO_NUM_12  // G5
#define BSP_LCD_DATA11  GPIO_NUM_13  // R0
#define BSP_LCD_DATA12  GPIO_NUM_14  // R1
#define BSP_LCD_DATA13  GPIO_NUM_21  // R2
#define BSP_LCD_DATA14  GPIO_NUM_4   // R3
#define BSP_LCD_DATA15  GPIO_NUM_40  // R4 (Проверь этот пин, он совпадает с DE?)

#endif // _BOARD_CONFIG_H_