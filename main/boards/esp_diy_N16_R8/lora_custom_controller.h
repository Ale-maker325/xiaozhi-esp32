#ifndef __LORA_CUSTOM_CONTROLLER_H__
#define __LORA_CUSTOM_CONTROLLER_H__

#include "mcp_server.h"
#include <RadioLib.h>
#include <esp_log.h>
#include "config.h"

static const char* RTAG = "LoraAI";

class LoraCustomController {
private:
    // Создаем модуль: CS, DIO1, RST, BUSY
    SX1262 radio = new Module(LORA_CS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN);

public:
    LoraCustomController() {
        ESP_LOGI(RTAG, "Запуск HT-Ra62...");

        // 1. Инициализируем шину SPI с твоими пинами
        SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);

        // 2. Базовая инициализация на нужной частобе
        int state = radio.begin(LORA_FREQ);

        if (state == RADIOLIB_ERR_NONE) {
            ESP_LOGI(RTAG, "Чип SX1262 найден. Настройка параметров...");

            // 3. Настройка TCXO (Важно для Ra62!)
            // Модуль требует питания на кварц. Обычно это 1.8В.
            // Параметр 1.8f берется из config.h
            state = radio.setTCXO(LORA_TCXO_VOLTAGE);
            if (state != RADIOLIB_ERR_NONE) {
                ESP_LOGE(RTAG, "Ошибка настройки TCXO: %d", state);
            }

            // 4. Настройка DIO2 как переключателя прием/передача (RF Switch)
            // Без этого антенна не будет переключаться в режим передачи
            state = radio.setDio2AsRfSwitch();
            if (state != RADIOLIB_ERR_NONE) {
                ESP_LOGE(RTAG, "Ошибка настройки DIO2 как RFSwitch: %d", state);
            }

            // 5. Тонкая настройка модема (Lora Modulation)
            // Spreading Factor 7, Bandwidth 125kHz, Coding Rate 5
            radio.setModemConfig(7, 125.0, 5);
            
            // Устанавливаем мощность 22 dBm (максимум)
            radio.setOutputPower(22);

            ESP_LOGI(RTAG, "LoRa полностью готова. Частота: %.1f MHz", LORA_FREQ);
        } else {
            ESP_LOGE(RTAG, "Критическая ошибка LoRa: %d", state);
        }

        // Регистрация инструмента в MCP (Джарвис)
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.lora.send", "Отправить радиокоманду на 433МГц", 
            PropertyList({
                Property("device", kPropertyTypeString, "Цель (насос, лампа)"),
                Property("action", kPropertyTypeString, "Действие (on/off)")
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string device = properties.at("device").value<std::string>();
                std::string action = properties.at("action").value<std::string>();
                std::string packet = device + ":" + action;
                
                ESP_LOGI(RTAG, "Передача: %s", packet.c_str());
                
                // Передача пакета
                int tx_state = radio.transmit(packet.c_str());

                if (tx_state == RADIOLIB_ERR_NONE) {
                    return "Команда успешно ушла в эфир.";
                } else {
                    return "Ошибка передачи пакета.";
                }
            });
    }
};

#endif // __LORA_CUSTOM_CONTROLLER_H__