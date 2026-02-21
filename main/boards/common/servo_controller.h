// #ifndef __SERVO_CONTROLLER_H__
// #define __SERVO_CONTROLLER_H__

// #include <driver/ledc.h>
// #include "mcp_server.h"
// #include <esp_log.h>
// #include <string>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// // --- КОНСТАНТЫ НАСТРОЙКИ ---
// // Стандарт для SG90 / MG90S: импульс от 500 до 2500 мкс соответствует 0-180 градусам
// #define SERVO_MIN_PULSEWIDTH_US (500)  // Минимальная ширина импульса (0°)
// #define SERVO_MAX_PULSEWIDTH_US (2500) // Максимальная ширина импульса (180°)
// #define SERVO_MAX_DEGREE        (180)  // Максимальный угол поворота
// #define SERVO_FREQ_HZ           (50)   // Частота обновления серво (50 Гц)

// /**
//  * Класс ServoController.
//  * Управляет сервоприводом через аппаратный ШИМ (LEDC) микроконтроллера ESP32-S3.
//  */
// class ServoController {
// private:
//     gpio_num_t pin_;             // Пин, к которому подключен сигнальный провод серво
//     ledc_channel_t channel_;     // Аппаратный канал LEDC (0-7)
//     int current_angle_ = 90;     // Храним текущее положение (старт с центра)

//     /**
//      * Вспомогательная функция для пересчета угла в значение Duty Cycle.
//      * Мы используем 13-битный таймер (разрешение от 0 до 8191).
//      */
//     uint32_t degree_to_duty(int degree) {
//         // Вычисляем длину импульса в микросекундах для заданного угла
//         uint32_t cal_pulsewidth = (SERVO_MIN_PULSEWIDTH_US + 
//             (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * degree) / SERVO_MAX_DEGREE));
        
//         // Формула: (2^Разрядность * Длина_Импульса) / Период
//         // Период при 50 Гц = 1/50 = 0.02 сек = 20000 мкс
//         return (uint32_t)((1 << 13) * cal_pulsewidth / 20000);
//     }

// public:
//     /**
//      * Конструктор контроллера.
//      * @param pin GPIO пин (например, GPIO_NUM_2).
//      * @param channel Канал LEDC (рекомендуется LEDC_CHANNEL_2, чтобы не конфликтовать с подсветкой).
//      */
//     ServoController(gpio_num_t pin, ledc_channel_t channel = LEDC_CHANNEL_2) 
//         : pin_(pin), channel_(channel) {
        
//         // 1. Настройка ТАЙМЕРА (Используем Таймер 2)
//         // Таймеры 0 и 1 обычно заняты дисплеем и светодиодами системы.
//         ledc_timer_config_t ledc_timer = {}; 
//         ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
//         ledc_timer.timer_num        = LEDC_TIMER_2; // Наш выделенный таймер
//         ledc_timer.duty_resolution  = LEDC_TIMER_13_BIT; // Высокая точность позиционирования
//         ledc_timer.freq_hz          = SERVO_FREQ_HZ;
//         ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
//         ledc_timer_config(&ledc_timer);

//         // 2. Настройка КАНАЛА (Привязываем Пин к Таймеру)
//         ledc_channel_config_t ledc_channel = {};
//         ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
//         ledc_channel.channel        = channel_;
//         ledc_channel.timer_sel      = LEDC_TIMER_2; // Привязка к Таймеру 2
//         ledc_channel.intr_type      = LEDC_INTR_DISABLE;
//         ledc_channel.gpio_num       = pin_;
//         ledc_channel.duty           = degree_to_duty(current_angle_);
//         ledc_channel.hpoint         = 0;
//         ledc_channel_config(&ledc_channel);

//         // Регистрируем функции в системе команд Джарвиса
//         RegisterMcpTool();
//         ESP_LOGI("Servo", "Контроллер запущен. Pin: %d, Timer: 2, Channel: %d", pin_, channel_);
//     }

//     /**
//      * Основная функция поворота.
//      * @param target_degree Целевой угол (0-180).
//      * @param speed_ms Задержка между шагами в мс (чем меньше, тем быстрее движение).
//      */
//     void SetAngle(int target_degree, int speed_ms = 15) {
//         // Защита от выхода за границы
//         if (target_degree < 0) target_degree = 0;
//         if (target_degree > 180) target_degree = 180;

//         /* === ВАРИАНТ ДЛЯ БЫСТРОГО ПОВОРОТА (ЗАКОММЕНТИРОВАНО ПО ЗАПРОСУ) ===
//          * Раскомментируй этот блок, если нужно мгновенное перемещение без плавности.
//          * * uint32_t duty = degree_to_duty(target_degree);
//          * ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
//          * ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
//          * current_angle_ = target_degree;
//          * return;
//          */

//         // === ПЛАВНЫЙ ПОВОРОТ ===
//         // Определяем, в какую сторону крутить (+1 градус или -1 градус)
//         int step = (target_degree > current_angle_) ? 1 : -1;

//         while (current_angle_ != target_degree) {
//             current_angle_ += step; // Делаем шаг в 1 градус
            
//             uint32_t duty = degree_to_duty(current_angle_);
//             ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
//             ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
            
//             // Ждем немного перед следующим градусом. 15мс — стандартная плавность.
//             if (speed_ms > 0) {
//                 vTaskDelay(pdMS_TO_TICKS(speed_ms));
//             }
//         }
//         ESP_LOGI("Servo", "Угол установлен: %d", current_angle_);
//     }

//     /**
//      * Жест приветствия (Wiggle).
//      * Сервопривод делает быстрое движение туда-обратно.
//      */
//     void Wiggle() {
//         ESP_LOGI("Servo", "Выполняю жест 'Приветствие'");
//         int start_pos = current_angle_; // Запоминаем текущую позицию
        
//         // Быстрая серия движений (скорость 5мс на градус)
//         SetAngle(130, 5); 
//         SetAngle(50, 5);  
//         SetAngle(130, 5); 
//         SetAngle(start_pos, 10); // Возвращаемся в исходное положение чуть медленнее
//     }

// private:
//     /**
//      * Регистрация инструментов для взаимодействия с AI через протокол MCP.
//      */
//     void RegisterMcpTool() {
//         auto& mcp = McpServer::GetInstance();
        
//         // Команда 1: Установка угла
//         mcp.AddTool("self.servo.set_angle", 
//             "Повернуть сервопривод (0-180). Параметр angle - целое число.", 
//             PropertyList({
//                 Property("angle", kPropertyTypeInteger, 0, 180) 
//             }), 
//             [this](const PropertyList& params) -> ReturnValue {
//                 try {
//                     int angle = params["angle"].value<int>();
//                     this->SetAngle(angle, 15); // Используем стандартную плавность
//                     return true;
//                 } catch (const std::exception& e) {
//                     return std::string("Ошибка: ") + e.what();
//                 }
//             });

//         // Команда 2: Приветствие (Жест)
//         mcp.AddTool("self.servo.wiggle", 
//             "Поздороваться или помахать сервоприводом.", 
//             PropertyList(), 
//             [this](const PropertyList& params) -> ReturnValue {
//                 this->Wiggle();
//                 return std::string("Привет! Я помахал тебе сервоприводом.");
//             });
            
//         // Команда 3: Получение состояния
//         mcp.AddTool("self.servo.get_state",
//             "Узнать текущий угол поворота сервопривода.",
//             PropertyList(),
//             [this](const PropertyList& params) -> ReturnValue {
//                 return std::string("Текущий угол: ") + std::to_string(this->current_angle_) + " градусов.";
//             });
//     }
// };

// #endif // __SERVO_CONTROLLER_H__




#ifndef __SERVO_CONTROLLER_H__
#define __SERVO_CONTROLLER_H__

#include <driver/ledc.h>
#include "mcp_server.h"
#include <esp_log.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- КОНСТАНТЫ НАСТРОЙКИ ---
// Стандарт для SG90 / MG90S: импульс от 500 до 2500 мкс соответствует 0-180 градусам
#define SERVO_MIN_PULSEWIDTH_US (500)  // Минимальная ширина импульса (0°)
#define SERVO_MAX_PULSEWIDTH_US (2500) // Максимальная ширина импульса (180°)
#define SERVO_MAX_DEGREE        (180)  // Максимальный угол поворота
#define SERVO_FREQ_HZ           (50)   // Частота обновления серво (50 Гц)

/**
 * Класс ServoController.
 * Управляет сервоприводом через аппаратный ШИМ (LEDC) микроконтроллера ESP32-S3.
 * Теперь поддерживает именование для подключения нескольких приводов.
 */
class ServoController {
private:
    gpio_num_t pin_;             // Пин, к которому подключен сигнальный провод серво
    ledc_channel_t channel_;     // Аппаратный канал LEDC (0-7)
    std::string name_;           // Имя привода (например, "head" или "hand")
    int current_angle_ = 90;     // Храним текущее положение (старт с центра)

    /**
     * Вспомогательная функция для пересчета угла в значение Duty Cycle.
     * Мы используем 13-битный таймер (разрешение от 0 до 8191).
     */
    uint32_t degree_to_duty(int degree) {
        uint32_t cal_pulsewidth = (SERVO_MIN_PULSEWIDTH_US + 
            (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * degree) / SERVO_MAX_DEGREE));
        
        return (uint32_t)((1 << 13) * cal_pulsewidth / 20000);
    }

public:
    /**
     * Конструктор контроллера.
     * @param pin GPIO пин.
     * @param name Уникальное имя (будет частью команды в MCP).
     * @param channel Канал LEDC (у каждого серво должен быть свой!).
     */
    ServoController(gpio_num_t pin, std::string name, ledc_channel_t channel = LEDC_CHANNEL_2) 
        : pin_(pin), channel_(channel), name_(name) {
        
        // 1. Настройка ТАЙМЕРА (Используем Таймер 2 - общий для всех серво)
        ledc_timer_config_t ledc_timer = {}; 
        ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
        ledc_timer.timer_num        = LEDC_TIMER_2; 
        ledc_timer.duty_resolution  = LEDC_TIMER_13_BIT; 
        ledc_timer.freq_hz          = SERVO_FREQ_HZ;
        ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
        ledc_timer_config(&ledc_timer);

        // 2. Настройка КАНАЛА (Индивидуальный для каждого привода)
        ledc_channel_config_t ledc_channel = {};
        ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel        = channel_;
        ledc_channel.timer_sel      = LEDC_TIMER_2; 
        ledc_channel.intr_type      = LEDC_INTR_DISABLE;
        ledc_channel.gpio_num       = pin_;
        ledc_channel.duty           = degree_to_duty(current_angle_);
        ledc_channel.hpoint         = 0;
        ledc_channel_config(&ledc_channel);

        // Регистрируем функции в системе Джарвиса
        RegisterMcpTool();
        ESP_LOGI("Servo", "Контроллер [%s] запущен. Pin: %d, Channel: %d", name_.c_str(), pin_, channel_);
    }

    /**
     * Основная функция поворота.
     */
    void SetAngle(int target_degree, int speed_ms = 15) {
        if (target_degree < 0) target_degree = 0;
        if (target_degree > 180) target_degree = 180;

        // ПЛАВНЫЙ ПОВОРОТ
        int step = (target_degree > current_angle_) ? 1 : -1;

        while (current_angle_ != target_degree) {
            current_angle_ += step; 
            
            uint32_t duty = degree_to_duty(current_angle_);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
            
            if (speed_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(speed_ms));
            }
        }
    }

    /**
     * Новые жесты
     */
    
    // Жест Согласия (Кивок)
    void Nod() {
        ESP_LOGI("Servo", "[%s] Выполняю жест 'Кивок'", name_.c_str());
        int start_pos = current_angle_;
        SetAngle(120, 10); 
        SetAngle(60, 10);  
        SetAngle(start_pos, 15);
    }

    // Жест Отрицания (Повороты влево-вправо)
    void Shake() {
        ESP_LOGI("Servo", "[%s] Выполняю жест 'Нет'", name_.c_str());
        int start_pos = current_angle_;
        SetAngle(start_pos + 30, 8); 
        SetAngle(start_pos - 30, 8);  
        SetAngle(start_pos, 12);
    }

    // Твой оригинальный жест Приветствия
    void Wiggle() {
        ESP_LOGI("Servo", "[%s] Выполняю жест 'Приветствие'", name_.c_str());
        int start_pos = current_angle_;
        SetAngle(130, 5); 
        SetAngle(50, 5);  
        SetAngle(130, 5); 
        SetAngle(start_pos, 10);
    }

private:
    /**
     * Регистрация инструментов. Теперь команды включают имя привода.
     */
    void RegisterMcpTool() {
        auto& mcp = McpServer::GetInstance();
        std::string base_cmd = "self.servo." + name_;
        
        // Инструмент: Установка угла (напр. self.servo.head.set_angle)
        mcp.AddTool(base_cmd + ".set_angle", 
            "Повернуть " + name_ + " (0-180).", 
            PropertyList({
                Property("angle", kPropertyTypeInteger, 0, 180) 
            }), 
            [this](const PropertyList& params) -> ReturnValue {
                this->SetAngle(params["angle"].value<int>(), 15);
                return true;
            });

        // Инструмент: Приветствие
        mcp.AddTool(base_cmd + ".wiggle", 
            "Помахать " + name_ + " (приветствие).", 
            PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue {
                this->Wiggle();
                return "Я помахал устройством " + name_;
            });

        // Инструмент: Кивок (Да)
        mcp.AddTool(base_cmd + ".nod", 
            "Кивнуть " + name_ + " (согласие).", 
            PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue {
                this->Nod();
                return name_ + " кивает в знак согласия.";
            });

        // Инструмент: Отрицание (Нет)
        mcp.AddTool(base_cmd + ".shake", 
            "Помотать " + name_ + " (отказ/нет).", 
            PropertyList(), 
            [this](const PropertyList& params) -> ReturnValue {
                this->Shake();
                return name_ + " мотает нет.";
            });
            
        // Инструмент: Получение состояния
        mcp.AddTool(base_cmd + ".get_state",
            "Узнать текущий угол " + name_,
            PropertyList(),
            [this](const PropertyList& params) -> ReturnValue {
                return "Угол " + name_ + ": " + std::to_string(this->current_angle_) + "°";
            });
    }
};

#endif // __SERVO_CONTROLLER_H__