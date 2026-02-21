#include "wifi_board.h"
#include "display/lcd_display.h" 
#include "esp_lcd_st7701.h"
#include "audio/codecs/box_audio_codec.h"
#include "config.h"
#include "esp_io_expander_tca9554.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "boards/common/backlight.h"

// Заголовки для работы с расширенным IO и LVGL
#include <esp_lcd_panel_io_additions.h>
#include <esp_lvgl_port.h>

#define TAG "EspDiyBoard"

class EspDiyN16R8Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t expander_;
    Display* display_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));
    }

    void InitializeLcd() {
        // 1. Настройка 3-wire SPI
        // ИСПРАВЛЕНО: Удалено поле lcd_bit_order, которого нет в структуре
        esp_lcd_panel_io_3wire_spi_config_t io_config = {
            .line_config = {
                .cs_gpio_num = LCD_SPI_CS,
                .scl_gpio_num = LCD_SPI_SCL,
                .sda_gpio_num = LCD_SPI_SDA,
            },
            .expect_clk_speed = 10 * 1000 * 1000,
            .spi_mode = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = { 
                .use_dc_bit = 1,
                .lsb_first = 0 // 0 = MSB First (обычно это дефолт)
            },
        };

        esp_lcd_panel_io_handle_t io_handle = NULL;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

        // 2. Настройка таймингов RGB
        esp_lcd_rgb_timing_t timings = {
            .pclk_hz = 12 * 1000 * 1000,
            .h_res = DISPLAY_WIDTH,
            .v_res = DISPLAY_HEIGHT,
            .hsync_pulse_width = 8,
            .hsync_back_porch = 50,
            .hsync_front_porch = 10,
            .vsync_pulse_width = 8,
            .vsync_back_porch = 20,
            .vsync_front_porch = 10,
            .flags = { .pclk_active_neg = 1 }
        };

        // 3. Конфигурация панели RGB (Соблюдаем порядок полей!)
        esp_lcd_rgb_panel_config_t rgb_cfg = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = timings,
            .data_width = 16,
            .bits_per_pixel = 16,
            .num_fbs = 1,
            .bounce_buffer_size_px = 0,
            .sram_trans_align = 0,
            .psram_trans_align = 64,
            .hsync_gpio_num = BSP_LCD_HSYNC,
            .vsync_gpio_num = BSP_LCD_VSYNC,
            .de_gpio_num = BSP_LCD_DE,     // Сначала DE
            .pclk_gpio_num = BSP_LCD_PCLK, // Потом PCLK
            .disp_gpio_num = GPIO_NUM_NC,
            .data_gpio_nums = {
                BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3,
                BSP_LCD_DATA4, BSP_LCD_DATA5, BSP_LCD_DATA6, BSP_LCD_DATA7,
                BSP_LCD_DATA8, BSP_LCD_DATA9, BSP_LCD_DATA10, BSP_LCD_DATA11,
                BSP_LCD_DATA12, BSP_LCD_DATA13, BSP_LCD_DATA14, BSP_LCD_DATA15
            },
            .flags = { .fb_in_psram = 1 }
        };

        // 4. Настройка вендора ST7701
        st7701_vendor_config_t vendor_cfg = {
            .init_cmds = NULL, 
            .init_cmds_size = 0,
            .rgb_config = &rgb_cfg, 
            .flags = {
                .mirror_by_cmd = 1,
                .enable_io_multiplex = 1 
            }
        };

        esp_lcd_panel_dev_config_t panel_cfg = {
            .reset_gpio_num = GPIO_NUM_NC,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .vendor_config = &vendor_cfg,
        };

        esp_lcd_panel_handle_t panel_handle = NULL;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_cfg, &panel_handle));
        
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

        // 5. Создание дисплея. Используем RgbLcdDisplay, т.к. LcdDisplay защищен.
        display_ = new RgbLcdDisplay(
            io_handle, 
            panel_handle, 
            DISPLAY_WIDTH, 
            DISPLAY_HEIGHT,
            0, 0,           
            false, false,   
            false           
        );
    }

public:
    EspDiyN16R8Board() {
        InitializeI2c();
        InitializeLcd();
    }

    virtual AudioCodec* GetAudioCodec() override {
        // 12 аргументов для BoxAudioCodec
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            GPIO_NUM_NC,          // MCLK
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC,          // PA_PIN
            0x10, 0x18,           // Адреса
            true                  // Reference
        );
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN);
        return &backlight;
    }

    virtual std::string GetBoardType() override { return "ESP_DIY_ST7701"; }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override {
        level = 100; charging = false; discharging = true;
        return true;
    }

    virtual NetworkInterface* GetNetwork() override {
        return WifiBoard::GetNetwork();
    }
};

void* create_board() {
    return new EspDiyN16R8Board();
}