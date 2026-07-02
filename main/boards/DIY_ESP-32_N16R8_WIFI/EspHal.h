#ifndef ESP_HAL_H
#define ESP_HAL_H

#include <RadioLib.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Класс-прослойка для работы RadioLib внутри ESP-IDF
class EspHal : public RadioLibHal {
private:
    spi_device_handle_t _spi_handle;
    spi_host_device_t _host;

public:
    // Конструктор принимает только SPI Host (например, SPI2_HOST)
    EspHal(spi_host_device_t host) : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING), _host(host) {}

    void init() override {
        // Инициализация GPIO и прочего не требуется здесь, 
        // так как RadioLib вызовет pinMode сам
    }

    // Управление GPIO через драйверы ESP-IDF
    void pinMode(uint32_t pin, uint32_t mode) override {
        if (pin == RADIOLIB_NC) return;
        gpio_reset_pin((gpio_num_t)pin);
        gpio_set_direction((gpio_num_t)pin, mode == OUTPUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (pin == RADIOLIB_NC) return;
        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        if (pin == RADIOLIB_NC) return 0;
        return gpio_get_level((gpio_num_t)pin);
    }

    // Тайминги
    void delay(unsigned long ms) override { vTaskDelay(pdMS_TO_TICKS(ms)); }
    void delayMicroseconds(unsigned long us) override { ets_delay_us(us); }
    unsigned long millis() override { return esp_timer_get_time() / 1000; }
    unsigned long micros() override { return esp_timer_get_time(); }

    // Реализация SPI интерфейса
    void spiBegin() override {
        // Конфигурация конкретного устройства на шине (SX1262)
        spi_device_interface_config_t devcfg = {
            .mode = 0,
            .clock_speed_hz = 2000000, // 2MHz для стабильности
            .spics_io_num = -1,        // CS управляем вручную через digitalWrite
            .queue_size = 7,
        };
        spi_bus_add_device(_host, &devcfg, &_spi_handle);
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        if (len == 0) return;
        spi_transaction_t t = {
            .length = 8 * len,
            .tx_buffer = out,
            .rx_buffer = in
        };
        spi_device_transmit(_spi_handle, &t);
    }

    void spiEnd() override {
        spi_bus_remove_device(_spi_handle);
    }
};

#endif