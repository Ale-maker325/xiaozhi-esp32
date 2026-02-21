// #ifndef __LAMP_CONTROLLER_H__
// #define __LAMP_CONTROLLER_H__

// #include "mcp_server.h"


// class LampController {
// private:
//     bool power_ = false;
//     gpio_num_t gpio_num_;

// public:
//     LampController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
//         gpio_config_t config = {
//             .pin_bit_mask = (1ULL << gpio_num_),
//             .mode = GPIO_MODE_OUTPUT,
//             .pull_up_en = GPIO_PULLUP_DISABLE,
//             .pull_down_en = GPIO_PULLDOWN_DISABLE,
//             .intr_type = GPIO_INTR_DISABLE,
//         };
//         ESP_ERROR_CHECK(gpio_config(&config));
//         gpio_set_level(gpio_num_, 0);

//         auto& mcp_server = McpServer::GetInstance();
//         mcp_server.AddTool("self.lamp.get_state", "Get the power state of the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
//             return power_ ? "{\"power\": true}" : "{\"power\": false}";
//         });

//         mcp_server.AddTool("self.lamp.turn_on", "Turn on the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
//             power_ = true;
//             gpio_set_level(gpio_num_, 1);
//             return true;
//         });

//         mcp_server.AddTool("self.lamp.turn_off", "Turn off the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
//             power_ = false;
//             gpio_set_level(gpio_num_, 0);
//             return true;
//         });
//     }
// };


// #endif // __LAMP_CONTROLLER_H__







/**
 * Блок include-guard (защита заголовка). 
 * Гарантирует, что если этот файл будет случайно подключен дважды в разных местах проекта,
 * компилятор не выдаст ошибку о повторном объявлении класса.
 */
#ifndef __LAMP_CONTROLLER_H__
#define __LAMP_CONTROLLER_H__

// Подключаем заголовочный файл сервера MCP для регистрации команд
#include "mcp_server.h"
// Подключаем стандартный драйвер ESP-IDF для работы с ножками (GPIO) микроконтроллера. // Тут лежит gpio_config и gpio_set_level
#include <driver/gpio.h>
// Подключаем стандартную библиотеку строк C++, чтобы работать с текстом (имена устройств)
#include <string>

/**
 * Класс LampController — это "чертеж" объекта, который умеет управлять одной конкретной 
 * железкой (лампой, паяльником и т.д.) и "рассказывать" о ней ИИ-модели через протокол MCP.
 */
class LampController {
private:
    /**
     * ПРИВАТНЫЕ ПЕРЕМЕННЫЕ (Состояние объекта)
     * Доступны только внутри этого класса.
     */
    
    // Хранит текущее состояние: true — включено, false — выключено. 
    // Нужно, чтобы мы всегда могли ответить ИИ-модели, в каком статусе сейчас прибор.
    bool power_ = false;

    // Хранит номер физического пина (ножки) ESP32, к которой подключено реле или транзистор.
    // Тип gpio_num_t — это специальный тип данных из ESP-IDF (например, GPIO_NUM_18).
    gpio_num_t gpio_num_;

    // Технический идентификатор устройства (например, "solder"). 
    // Используется для формирования имени команды в коде: "self.solder.turn_on".
    std::string device_id_;

    // Человекопонятное имя (например, "Паяльник"). 
    // Передается ИИ-модели, чтобы она понимала, что именно она включает.
    std::string device_name_;

public:
    /**
     * КОНСТРУКТОР КЛАССА
     * Вызывается один раз при создании объекта. 
     * Принимает: номер пина, ID (для кода) и Имя (для людей/ИИ).
     */
    LampController(gpio_num_t gpio_num, const std::string& id, const std::string& name) 
        // Список инициализации: записываем переданные значения в наши приватные переменные
        : gpio_num_(gpio_num), device_id_(id), device_name_(name) {
        
        /**
         * 1. НАСТРОЙКА ЖЕЛЕЗА (GPIO)
         */
        
        // Создаем структуру конфигурации GPIO. Это стандартный способ настройки в ESP32.
        gpio_config_t config = {
            // Указываем, какой именно пин мы настраиваем. 
            // 1ULL << пин — это создание "битовой маски" (установка нужного бита в единицу).
            .pin_bit_mask = (1ULL << gpio_num_),
            
            // Устанавливаем режим работы: только на выход (будем подавать напряжение сами).
            .mode = GPIO_MODE_OUTPUT,
            
            // Отключаем внутренние подтяжки к питанию (Pull-up) и к земле (Pull-down).
            // Для управления реле/модулями они обычно не нужны.
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            
            // Отключаем прерывания на этом пине (они нужны только для входов/кнопок).
            .intr_type = GPIO_INTR_DISABLE,
        };

        // Применяем настройки. ESP_ERROR_CHECK — это макрос, который перезагрузит ESP32,
        // если настройка не удалась (например, если указан несуществующий пин).
        ESP_ERROR_CHECK(gpio_config(&config));

        // Принудительно устанавливаем 0 (низкий уровень) при старте, чтобы ничего
        // случайно не включилось само при подаче питания на плату.
        gpio_set_level(gpio_num_, 0);

        /**
         * 2. РЕГИСТРАЦИЯ В MCP SERVER (Логика для ИИ)
         */
        
        // Получаем ссылку на единственный экземпляр MCP сервера (паттерн Singleton).
        auto& mcp_server = McpServer::GetInstance();

        /**
         * ИНСТРУМЕНТ 1: ПОЛУЧЕНИЕ СОСТОЯНИЯ
         * Формирует команду: "self.[id].get_state"
         */
        mcp_server.AddTool("self." + device_id_ + ".get_state", 
            "Узнать состояние устройства: " + device_name_, 
            PropertyList(), // Список параметров пустой (нам ничего не нужно передавать, чтобы спросить)
            // Лямбда-выражение (анонимная функция), которая выполнится, когда ИИ вызовет эту команду
            // Она возвращает строку в формате JSON, чтобы ИИ мог легко понять, включено устройство или нет.
            // Например, возвращаем {"power": true} или {"power": false}
            //this — это адрес текущего объекта (твоего паяльника или фена).
            //(const PropertyList& properties) (Входящие данные): Это параметры команды. Даже если они нам не нужны (как в случае с «узнать состояние»), сервер всё равно их передает «на всякий случай». Мы их просто игнорируем.
            [this](const PropertyList& properties) -> ReturnValue {
                // Возвращаем ответ в формате JSON, который ИИ-модель сможет легко распарсить.
                return power_ ? "{\"power\": true}" : "{\"power\": false}";
            });

        /**
         * ИНСТРУМЕНТ 2: ВКЛЮЧЕНИЕ
         * Формирует команду: "self.[id].turn_on"
         */
        mcp_server.AddTool("self." + device_id_ + ".turn_on", 
            "Включить " + device_name_, 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                // 1. Меняем внутреннюю переменную
                power_ = true;
                // 2. Подаем физический сигнал (высокий уровень 3.3В) на ножку процессора
                gpio_set_level(gpio_num_, 1);
                // 3. Возвращаем "true" серверу, чтобы он знал — команда прошла успешно
                return true;
            });

        /**
         * ИНСТРУМЕНТ 3: ВЫКЛЮЧЕНИЕ
         * Формирует команду: "self.[id].turn_off"
         */
        mcp_server.AddTool("self." + device_id_ + ".turn_off", 
            "Выключить " + device_name_, 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                power_ = false;
                // Подаем физический 0 (земля) на ножку процессора
                gpio_set_level(gpio_num_, 0);
                return true;
            });
    }
};

#endif // Завершение блока __LAMP_CONTROLLER_H__