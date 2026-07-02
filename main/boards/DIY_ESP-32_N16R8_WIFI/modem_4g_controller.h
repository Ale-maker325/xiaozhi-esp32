#ifndef __MODEM_4G_CONTROLLER_H__
#define __MODEM_4G_CONTROLLER_H__

#include <driver/uart.h>
#include <esp_log.h>
#include <string>
#include "mcp_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Modem4gController {
public:
    Modem4gController(int tx_pin, int rx_pin) {
        ESP_LOGI("Modem4g", "Инициализация 4G модема на UART2...");
        
        // 1. НАСТРОЙКА ЖЕЛЕЗА (UART)
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_APB,
        };
        
        ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 2048, 0, 0, NULL, 0));

        // ВАЖНО: Запускаем процесс пробуждения модема в отдельной задаче, 
        // чтобы не блокировать загрузку всей платы!
        xTaskCreate(ModemInitTask, "modem_init_task", 4096, this, 5, NULL);

        // 3. РЕГИСТРАЦИЯ ИНСТРУМЕНТОВ В MCP-СЕРВЕРЕ
        auto& mcp = McpServer::GetInstance();
        
        mcp.AddTool("self.modem.get_signal", "Сигнал (AT+CSQ)", PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue { return this->SendAt("AT+CSQ"); });
            
        mcp.AddTool("self.modem.get_status", "Регистрация LTE (AT+CEREG?)", PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue { return this->SendAt("AT+CEREG?"); });

        mcp.AddTool("self.modem.check_pin", "Проверка блокировки SIM (AT+CPIN?)", PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue { return this->SendAt("AT+CPIN?"); });
            
        mcp.AddTool("self.modem.operator", "Поиск оператора сети (AT+COPS?)", PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue { return this->SendAt("AT+COPS?"); });
            
        mcp.AddTool("self.modem.set_apn", "Прописать APN", PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue { 
                return this->SendAt("AT+CGDCONT=1,\"IP\",\"internet\""); 
            });

        mcp.AddTool("self.modem.send_command", "Отправить кастомную AT-команду", PropertyList({
                Property("cmd", kPropertyTypeString, "Текст команды")
            }), 
            [this](const PropertyList& params) -> ReturnValue { 
                return this->SendAt(params["cmd"].value<std::string>().c_str()); 
            });
    }

    // Публичный метод для будущего переключения в интернет-режим
    void EnableDataMode(bool enable) {
        if (enable) {
            ESP_LOGI("Modem4g", "Команда на активацию интернета...");
            SendAt("AT+CGACT=1,1"); // Активируем профиль данных
        } else {
            ESP_LOGI("Modem4g", "Возврат в режим ожидания...");
        }
    }

private:
    /**
     * @brief Фоновая задача для безопасного старта модема.
     * Ожидает физической загрузки модема перед отправкой первой команды.
     */
    static void ModemInitTask(void* arg) {
        Modem4gController* self = (Modem4gController*)arg;
        
        ESP_LOGI("Modem4g", "Ожидание 5 секунд для загрузки железа ML307...");
        vTaskDelay(pdMS_TO_TICKS(5000)); 
        
        ESP_LOGI("Modem4g", "Пробуждение модема (AT+CFUN=1)...");
        self->SendAt("AT+CFUN=1");
        
        // Уничтожаем задачу, она больше не нужна
        vTaskDelete(NULL);
    }

    std::string SendAt(const char* cmd) {
        uart_flush(UART_NUM_2);
        uart_write_bytes(UART_NUM_2, cmd, strlen(cmd));
        uart_write_bytes(UART_NUM_2, "\r\n", 2);
        
        std::string response = "";
        uint8_t buffer[128];
        int timeout_ms = 3000;
        
        while (timeout_ms > 0) {
            int length = uart_read_bytes(UART_NUM_2, buffer, sizeof(buffer) - 1, 100 / portTICK_PERIOD_MS);
            if (length > 0) {
                buffer[length] = '\0';
                response += (char*)buffer;
                if (response.find("OK") != std::string::npos || response.find("ERROR") != std::string::npos) {
                    break;
                }
            }
            timeout_ms -= 100;
        }

        if (response.empty()) {
            ESP_LOGE("Modem4g", "Команда [%s] -> ТАЙМАУТ (Модем молчит)", cmd);
            return "Ошибка: таймаут, нет ответа от железа.";
        }
        
        std::string clean_log = response;
        for (char& c : clean_log) { if (c == '\r' || c == '\n') c = ' '; }
        ESP_LOGI("Modem4g", "Команда [%s] -> Ответ: %s", cmd, clean_log.c_str());
        return response;
    }
};

#endif // __MODEM_4G_CONTROLLER_H__