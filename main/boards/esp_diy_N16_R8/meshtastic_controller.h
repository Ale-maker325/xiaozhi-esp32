#ifndef __MESHTASTIC_CONTROLLER_H__
#define __MESHTASTIC_CONTROLLER_H__

#include "mcp_server.h"
#include "application.h"
#include <HardwareSerial.h>
#include <esp_log.h>

#define TAG_MESH "Meshtastic"

class MeshtasticController {
private:
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    bool initialized_ = false;

    void BeginSerial() {
        if (!initialized_) {
            Serial2.begin(115200, SERIAL_8N1, (int)rx_pin_, (int)tx_pin_);
            initialized_ = true;
            
            // Запускаем фоновую задачу для прослушивания эфира
            xTaskCreate(UartListenerTask, "mesh_listener", 4096, this, 5, NULL);
        }
    }

    // Фоновая задача для чтения UART
    static void UartListenerTask(void* pvParameters) {
        MeshtasticController* self = (MeshtasticController*)pvParameters;
        while (true) {
            if (Serial2.available()) {
                // Читаем входящую строку от Meshtastic
                String incoming = Serial2.readStringUntil('\n');
                incoming.trim();

                if (incoming.length() > 0) {
                    ESP_LOGI(TAG_MESH, "Новое сообщение из эфира: %s", incoming.c_str());
                    
                    // Передаем текст Джарвису для озвучки
                    auto& app = Application::GetInstance();
                    app.Speak("Сэр, получено радиосообщение: " + std::string(incoming.c_str()));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Проверка каждые 100мс
        }
    }

public:
    MeshtasticController(gpio_num_t tx_pin, gpio_num_t rx_pin) 
        : tx_pin_(tx_pin), rx_pin_(rx_pin) {
        
        // Инициализируем порт сразу, чтобы начать слушать эфир
        BeginSerial();

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.lora.send", "Отправить текстовое сообщение в радиосеть Meshtastic", 
            PropertyList({
                Property("text", kPropertyTypeString, "Текст сообщения")
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string message = properties.at("text").value<std::string>();
                Serial2.println(message.c_str());
                return "Сообщение отправлено в эфир.";
            });
    }
};

#endif // __MESHTASTIC_CONTROLLER_H__