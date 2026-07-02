#ifndef __MESHTASTIC_CONTROLLER_H__
#define __MESHTASTIC_CONTROLLER_H__

#include "mcp_server.h"
#include "application.h"
#include <esp_log.h>
#include <string>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG_MESH "Meshtastic"
#define UART_NUM UART_NUM_2
#define BUF_SIZE (1024)

class MeshtasticController {
private:
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    bool initialized_ = false;

    void BeginSerial() {
        if (!initialized_) {
            uart_config_t uart_config = {
                .baud_rate = 115200,
                .data_bits = UART_DATA_8_BITS,
                .parity    = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .source_clk = UART_SCLK_DEFAULT,
            };
            uart_param_config(UART_NUM, &uart_config);
            uart_set_pin(UART_NUM, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
            initialized_ = true;
            xTaskCreate(UartListenerTask, "mesh_listener", 4096, this, 5, NULL);
        }
    }

    static void UartListenerTask(void* pvParameters) {
        MeshtasticController* self = (MeshtasticController*)pvParameters;
        uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
        while (true) {
            int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
            if (len > 0) {
                data[len] = '\0';
                std::string incoming((char*)data);
                
                // В проекте Xiaozhi вывод звука обычно идет через PlayNotification или задачу в App
                ESP_LOGI(TAG_MESH, "Сообщение: %s", incoming.c_str());
                // Если Speak не найден, используем логирование, пока не уточним метод API
                // Application::GetInstance().StartTask(new SpeakTask("Радио: " + incoming));
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        free(data);
    }

public:
    MeshtasticController(gpio_num_t tx_pin, gpio_num_t rx_pin) 
        : tx_pin_(tx_pin), rx_pin_(rx_pin) {
        BeginSerial();

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.lora.send", "Отправить текст в Meshtastic", 
            PropertyList({
                Property("text", kPropertyTypeString, "Текст сообщения")
            }), 
            [this](const ParameterList& parameters) -> ReturnValue { // В Xiaozhi часто ParameterList вместо PropertyList
                // Пытаемся достать значение через оператор []
                std::string message = parameters["text"].str(); 
                message += "\n";
                uart_write_bytes(UART_NUM, message.c_str(), message.length());
                return "Сообщение отправлено.";
            });
    }
};

#endif