#ifndef __LORA_CUSTOM_CONTROLLER_H__
#define __LORA_CUSTOM_CONTROLLER_H__

#include "mcp_server.h"
#include <RadioLib.h>
#include <esp_log.h>
#include "config.h"
#include "EspHal.h"

static const char* RTAG = "LoraAI";

class LoraCustomController {
private:
    EspHal* hal;
    SX1262* radio;

public:
    LoraCustomController() {
        ESP_LOGI(RTAG, "Запуск LoRa для Meshtastic плат...");

        // 1. Сначала настраиваем саму шину SPI (общую для всех устройств)
        spi_bus_config_t buscfg = {
            .mosi_io_num = LORA_MOSI_PIN,
            .miso_io_num = LORA_MISO_PIN,
            .sclk_io_num = LORA_SCK_PIN,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0
        };
        // Для ESP32-S3 Zero обычно используется SPI2_HOST
        spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

        // 2. Инициализируем HAL и модуль
        hal = new EspHal(SPI2_HOST);
        Module* mod = new Module(hal, LORA_CS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN);
        radio = new SX1262(mod);

        // 3. Проверка чипа
        int state = radio->begin(LORA_FREQ);

        if (state == RADIOLIB_ERR_NONE) {
            ESP_LOGI(RTAG, "SX1262 найден!");
            radio->setTCXO(LORA_TCXO_VOLTAGE);
            radio->setDio2AsRfSwitch();
            radio->setOutputPower(22);
        } else {
            ESP_LOGE(RTAG, "Ошибка инициализации LoRa: %d", state);
        }

        // 4. Регистрация в MCP (Джарвис)
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.lora.send", "Отправить команду через LoRa", 
            PropertyList({
                Property("device", kPropertyTypeString, "Цель"),
                Property("action", kPropertyTypeString, "Действие")
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string msg = properties.at("device").value<std::string>() + ":" + 
                                  properties.at("action").value<std::string>();
                
                ESP_LOGI(RTAG, "Передача: %s", msg.c_str());
                int tx_state = radio->transmit(msg.c_str());
                
                return (tx_state == RADIOLIB_ERR_NONE) ? "Сигнал ушел." : "Ошибка связи.";
            });
    }
};

#endif