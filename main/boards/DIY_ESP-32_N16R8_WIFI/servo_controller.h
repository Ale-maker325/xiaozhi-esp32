// #ifndef __SERVO_CONTROLLER_H__
// #define __SERVO_CONTROLLER_H__

// #include <driver/ledc.h>
// #include "mcp_server.h"
// #include <esp_log.h>
// #include <string>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// // --- КОНСТАНТЫ НАСТРОЙКИ ---
// #define SERVO_MIN_PULSEWIDTH_US (500)  // Импульс для положения 0 градусов (мкс)
// #define SERVO_MAX_PULSEWIDTH_US (2500) // Импульс для положения 180 градусов (мкс)
// #define SERVO_MAX_DEGREE        (180)  // Лимит аппаратного поворота сервопривода
// #define SERVO_FREQ_HZ           (50)   // Частота управляющего ШИМ-сигнала (50 Гц)

// /**
//  * Класс ServoController.
//  * Потокобезопасное асинхронное управление сервоприводами на базе подсистемы LEDC микроконтроллера ESP32-S3.
//  */
// class ServoController {
// private:
//     gpio_num_t pin_;             // Выделенный физический пин сигнального провода
//     ledc_channel_t channel_;     // Аппаратный канал генерации ШИМ
//     std::string name_;           // Символьное имя узла анимации (например, "head")
//     int current_angle_ = 90;     // Переменная отслеживания текущей позиции

//     /**
//      * Конвертация угла в дискретные единицы заполнения ШИМ (Duty Cycle) для 13-битного таймера.
//      */
//     uint32_t degree_to_duty(int degree) {
//         uint32_t cal_pulsewidth = (SERVO_MIN_PULSEWIDTH_US + 
//             (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * degree) / SERVO_MAX_DEGREE));
        
//         return (uint32_t)((1 << 13) * cal_pulsewidth / 20000);
//     }

//     /**
//      * Вспомогательная структура передачи параметров в фоновую FreeRTOS задачу.
//      */
//     struct MoveArgs {
//         ServoController* instance;
//         int target_degree;
//         int speed_ms;
//     };

//     /**
//      * Статический рабочий поток FreeRTOS для реализации плавного неблокирующего перемещения.
//      */
//     static void ServoMoveTask(void* pvParameters) {
//         MoveArgs* args = static_cast<MoveArgs*>(pvParameters);
//         ServoController* servo = args->instance;

//         int step = (args->target_degree > servo->current_angle_) ? 1 : -1;

//         while (servo->current_angle_ != args->target_degree) {
//             servo->current_angle_ += step; 
            
//             uint32_t duty = servo->degree_to_duty(servo->current_angle_);
//             ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel_, duty);
//             ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel_);
            
//             if (args->speed_ms > 0) {
//                 vTaskDelay(pdMS_TO_TICKS(args->speed_ms));
//             }
//         }
        
//         // Освобождаем динамическую память выделенной структуры и завершаем поток
//         delete args;
//         vTaskDelete(NULL);
//     }

// public:
//     /**
//      * Конструктор контроллера сервопривода.
//      */
//     ServoController(gpio_num_t pin, std::string name, ledc_channel_t channel = LEDC_CHANNEL_2) 
//         : pin_(pin), channel_(channel), name_(name) {
        
//         // Блокировка повторной конфигурации LEDC_TIMER_2 при создании нескольких объектов
//         static bool timer_initialized = false;
//         if (!timer_initialized) {
//             ledc_timer_config_t ledc_timer = {}; 
//             ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
//             ledc_timer.timer_num        = LEDC_TIMER_2; 
//             ledc_timer.duty_resolution  = LEDC_TIMER_13_BIT; 
//             ledc_timer.freq_hz          = SERVO_FREQ_HZ;
//             ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
//             ledc_timer_config(&ledc_timer);
//             timer_initialized = true;
//             ESP_LOGI("Servo", "Базовый LEDC_TIMER_2 успешно инициализирован один раз для всех приводов.");
//         }

//         // Индивидуальная привязка пина к выбранному аппаратному ШИМ-каналу
//         ledc_channel_config_t ledc_channel = {};
//         ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
//         ledc_channel.channel        = channel_;
//         ledc_channel.timer_sel      = LEDC_TIMER_2; 
//         ledc_channel.intr_type      = LEDC_INTR_DISABLE;
//         ledc_channel.gpio_num       = pin_;
//         ledc_channel.duty           = degree_to_duty(current_angle_);
//         ledc_channel.hpoint         = 0;
//         ledc_channel_config(&ledc_channel);

//         RegisterMcpTool();
//         ESP_LOGI("Servo", "Контроллер [%s] успешно запущен на Pin: %d, Channel: %d", name_.c_str(), pin_, channel_);
//     }

//     /**
//      * Запуск плавного перемещения в асинхронном режиме (без зависания системы).
//      */
//     void SetAngle(int target_degree, int speed_ms = 15) {
//         if (target_degree < 0) target_degree = 0;
//         if (target_degree > 180) target_degree = 180;

//         // Выделяем память под аргументы задачи
//         MoveArgs* args = new MoveArgs{this, target_degree, speed_ms};

//         // Создаем временную фоновую задачу во FreeRTOS для выполнения движения
//         std::string task_name = "srv_mv_" + name_;
//         xTaskCreate(ServoMoveTask, task_name.c_str(), 3072, args, 5, NULL);
//     }

//     // --- СЕКЦИЯ СКРИПТОВЫХ АНИМАЦИЙ И ЖЕСТОВ ---
    
//     // Синхронный внутренний метод (вызывается строго внутри комплексных жестов)
//     void SetAngleSync(int target_degree, int speed_ms = 15) {
//         if (target_degree < 0) target_degree = 0;
//         if (target_degree > 180) target_degree = 180;
//         int step = (target_degree > current_angle_) ? 1 : -1;
//         while (current_angle_ != target_degree) {
//             current_angle_ += step; 
//             uint32_t duty = degree_to_duty(current_angle_);
//             ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
//             ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
//             if (speed_ms > 0) vTaskDelay(pdMS_TO_TICKS(speed_ms));
//         }
//     }

//     static void GestureTaskWrapper(void* pvParameters) {
//         std::pair<ServoController*, int>* data = static_cast<std::pair<ServoController*, int>*>(pvParameters);
//         ServoController* servo = data->first;
//         int gesture = data->second;
//         int start_pos = servo->current_angle_;

//         if (gesture == 1) { // Жест 'Кивок'
//             servo->SetAngleSync(120, 10); 
//             servo->SetAngleSync(60, 10);  
//             servo->SetAngleSync(start_pos, 15);
//         } else if (gesture == 2) { // Жест 'Нет'
//             servo->SetAngleSync(start_pos + 30, 8); 
//             servo->SetAngleSync(start_pos - 30, 8);  
//             servo->SetAngleSync(start_pos, 12);
//         } else if (gesture == 3) { // Жест 'Приветствие'
//             servo->SetAngleSync(130, 5); 
//             servo->SetAngleSync(50, 5);  
//             servo->SetAngleSync(130, 5); 
//             servo->SetAngleSync(start_pos, 10);
//         }

//         delete data;
//         vTaskDelete(NULL);
//     }

//     void Nod() {
//         auto* data = new std::pair<ServoController*, int>(this, 1);
//         xTaskCreate(GestureTaskWrapper, "srv_nod", 3072, data, 5, NULL);
//     }

//     void Shake() {
//         auto* data = new std::pair<ServoController*, int>(this, 2);
//         xTaskCreate(GestureTaskWrapper, "srv_shk", 3072, data, 5, NULL);
//     }

//     void Wiggle() {
//         auto* data = new std::pair<ServoController*, int>(this, 3);
//         xTaskCreate(GestureTaskWrapper, "srv_wgl", 3072, data, 5, NULL);
//     }

// private:
//     /**
//      * Автоматическая публикация интерфейса управления в MCP-сервер.
//      */
//     void RegisterMcpTool() {
//         auto& mcp = McpServer::GetInstance();
//         std::string base_cmd = "self.servo." + name_;
        
//         // Инструмент установки произвольного угла
//         mcp.AddTool(base_cmd + ".set_angle", 
//             "Повернуть " + name_ + " на заданный угол (0-180 градусов).", 
//             PropertyList({
//                 Property("angle", kPropertyTypeInteger, 0, 180) 
//             }), 
//             [this](const PropertyList& params) -> ReturnValue {
//                 this->SetAngle(params["angle"].value<int>(), 15);
//                 return true; // Мгновенный ответ серверу, пока серва крутится в фоне
//             });

//         // Инструмент приветственного взмаха
//         mcp.AddTool(base_cmd + ".wiggle", 
//             "Выполнить жест приветствия приводом " + name_, 
//             PropertyList(), 
//             [this](const PropertyList& params) -> ReturnValue {
//                 this->Wiggle();
//                 return "Запущена анимация приветствия для устройства " + name_;
//             });

//         // Инструмент жеста согласия
//         mcp.AddTool(base_cmd + ".nod", 
//             "Кивнуть приводом " + name_ + " (выражение согласия/Да).", 
//             PropertyList(), 
//             [this](const PropertyList& params) -> ReturnValue {
//                 this->Nod();
//                 return name_ + " начинает кивать.";
//             });

//         // Инструмент жеста отрицания
//         mcp.AddTool(base_cmd + ".shake", 
//             "Помотать приводом " + name_ + " из стороны в сторону (выражение отказа/Нет).", 
//             PropertyList(), 
//             [this](const PropertyList& params) -> ReturnValue {
//                 this->Shake();
//                 return name_ + " имитирует жест отрицания.";
//             });
            
//         // Инструмент сбора телеметрии положения
//         mcp.AddTool(base_cmd + ".get_state",
//             "Запросить текущий угол положения привода " + name_,
//             PropertyList(),
//             [this](const PropertyList& params) -> ReturnValue {
//                 return "Текущий угол " + name_ + " составляет: " + std::to_string(this->current_angle_) + "°";
//             });
//     }
// };

// #endif // __SERVO_CONTROLLER_H__








/**
 * @file servo_controller.h
 * @brief Потокобезопасное асинхронное управление сервоприводами.
 * Обеспечивает плавное движение сервоприводов без блокировки основного цикла программы.
 * Интегрирован с MCP-сервером для управления со стороны ИИ-модели.
 */

#ifndef __SERVO_CONTROLLER_H__
#define __SERVO_CONTROLLER_H__

#include <driver/ledc.h>
#include "mcp_server.h"
#include <esp_log.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// =============================================================================
// --- КОНСТАНТЫ НАСТРОЙКИ СЕРВОПРИВОДОВ ---
// =============================================================================
#define SERVO_MIN_PULSEWIDTH_US (500)  // Минимальный импульс (для 0 градусов) в микросекундах
#define SERVO_MAX_PULSEWIDTH_US (2500) // Максимальный импульс (для 180 градусов) в микросекундах
#define SERVO_MAX_DEGREE        (180)  // Аппаратный лимит угла поворота сервопривода
#define SERVO_FREQ_HZ           (50)   // Стандартная частота ШИМ для сервоприводов (50 Гц)

class ServoController {
private:
    gpio_num_t pin_;             // Физический пин микроконтроллера, к которому подключен сигнальный провод
    ledc_channel_t channel_;     // Аппаратный ШИМ-канал (LEDC_CHANNEL_0, LEDC_CHANNEL_1 и т.д.)
    std::string device_id_;      // Системный идентификатор (например, "head" или "box_lid")
    std::string device_name_;    // Человекопонятное имя для ИИ (например, "Голова" или "Крышка шкатулки")
    int current_angle_ = 90;     // Текущий угол (стартуем всегда с нейтрального положения 90 градусов)
    bool enable_gestures_;       // Флаг: разрешить ли регистрацию анимаций (кивки, махания) для этого привода

    /**
     * @brief Конвертирует угол (0-180) в значение скважности (Duty Cycle) для 13-битного таймера.
     * Формула рассчитывает длину импульса в микросекундах и переводит её в тики таймера.
     */
    uint32_t degree_to_duty(int degree) {
        uint32_t cal_pulsewidth = (SERVO_MIN_PULSEWIDTH_US + 
            (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * degree) / SERVO_MAX_DEGREE));
        
        // 13 бит = 8192 тика. 50 Гц = 20000 мкс (период).
        return (uint32_t)((1 << 13) * cal_pulsewidth / 20000);
    }

    // =============================================================================
    // --- ВНУТРЕННИЕ СТРУКТУРЫ И ЗАДАЧИ FREERTOS ---
    // =============================================================================

    /**
     * Структура для безопасной передачи параметров в фоновую задачу FreeRTOS.
     */
    struct MoveArgs {
        ServoController* instance; // Указатель на текущий объект (чтобы знать, какой мотор крутить)
        int target_degree;         // Целевой угол
        int speed_ms;              // Задержка в мс между градусами (скорость)
    };

    /**
     * @brief ФОНОВАЯ ЗАДАЧА: Плавное перемещение к заданному углу.
     * Работает асинхронно, не тормозит основной код.
     */
    static void ServoMoveTask(void* pvParameters) {
        // Извлекаем переданные аргументы
        MoveArgs* args = static_cast<MoveArgs*>(pvParameters);
        ServoController* servo = args->instance;

        // Определяем направление шага (+1 градус или -1 градус)
        int step = (args->target_degree > servo->current_angle_) ? 1 : -1;

        // Плавно двигаем серву, пока не достигнем цели
        while (servo->current_angle_ != args->target_degree) {
            servo->current_angle_ += step; 
            
            // Обновляем аппаратный ШИМ-сигнал
            uint32_t duty = servo->degree_to_duty(servo->current_angle_);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel_, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel_);
            
            // Пауза между шагами для обеспечения плавности
            if (args->speed_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(args->speed_ms));
            }
        }
        
        // Обязательная очистка памяти и завершение задачи, чтобы избежать утечек
        delete args;
        vTaskDelete(NULL);
    }

public:
    /**
     * @brief Конструктор контроллера сервопривода.
     * @param pin Вывод GPIO, к которому подключен сигнальный провод.
     * @param id Системный ID для формирования команд (англ. без пробелов, напр. "box").
     * @param name Понятное имя для ИИ (напр. "Крышка коробки").
     * @param channel Аппаратный канал LEDC (у каждого мотора должен быть свой).
     * @param enable_gestures Включает (true) регистрацию команд анимации (по умолчанию выключено false).
     */
    ServoController(gpio_num_t pin, const std::string& id, const std::string& name, 
                    ledc_channel_t channel, bool enable_gestures = false) 
        : pin_(pin), channel_(channel), device_id_(id), device_name_(name), enable_gestures_(enable_gestures) {
        
        // 1. НАСТРОЙКА БАЗОВОГО ТАЙМЕРА (Выполняется только один раз для всех сервоприводов)
        static bool timer_initialized = false;
        if (!timer_initialized) {
            ledc_timer_config_t ledc_timer = {}; 
            ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
            ledc_timer.timer_num        = LEDC_TIMER_2;        // Используем Таймер 2 для всех серв
            ledc_timer.duty_resolution  = LEDC_TIMER_13_BIT;   // Высокое разрешение для плавности
            ledc_timer.freq_hz          = SERVO_FREQ_HZ;       // 50 Гц (стандарт)
            ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
            ledc_timer_config(&ledc_timer);
            timer_initialized = true;
            ESP_LOGI("Servo", "Базовый LEDC_TIMER_2 успешно инициализирован.");
        }

        // 2. ПРИВЯЗКА КАНАЛА И ПИНА
        ledc_channel_config_t ledc_channel_cfg = {};
        ledc_channel_cfg.speed_mode     = LEDC_LOW_SPEED_MODE;
        ledc_channel_cfg.channel        = channel_;
        ledc_channel_cfg.timer_sel      = LEDC_TIMER_2; 
        ledc_channel_cfg.intr_type      = LEDC_INTR_DISABLE;
        ledc_channel_cfg.gpio_num       = pin_;
        ledc_channel_cfg.duty           = degree_to_duty(current_angle_);
        ledc_channel_cfg.hpoint         = 0;
        ledc_channel_config(&ledc_channel_cfg);

        // 3. РЕГИСТРАЦИЯ КОМАНД ДЛЯ ИИ
        RegisterMcpTool();
        ESP_LOGI("Servo", "Контроллер [%s] запущен. Пин: %d, Канал: %d, Анимации: %s", 
                 device_name_.c_str(), pin_, channel_, enable_gestures_ ? "ВКЛ" : "ВЫКЛ");
    }

    /**
     * @brief Запуск плавного перемещения в асинхронном режиме.
     * @param target_degree Желаемый угол (0-180).
     * @param speed_ms Скорость (задержка в мс на каждый градус).
     */
    void SetAngle(int target_degree, int speed_ms = 15) {
        // Защита от выхода за пределы допустимых углов
        if (target_degree < 0) target_degree = 0;
        if (target_degree > SERVO_MAX_DEGREE) target_degree = SERVO_MAX_DEGREE;

        // Выделяем память под аргументы для потока
        MoveArgs* args = new MoveArgs{this, target_degree, speed_ms};

        // Запускаем задачу. Если системе не хватит памяти, удаляем выделенный объект.
        std::string task_name = "srv_mv_" + device_id_;
        if (xTaskCreate(ServoMoveTask, task_name.c_str(), 3072, args, 5, NULL) != pdPASS) {
            ESP_LOGE("Servo", "Не удалось создать задачу перемещения для %s", device_name_.c_str());
            delete args;
        }
    }

    // =============================================================================
    // --- СЕКЦИЯ СКРИПТОВЫХ АНИМАЦИЙ (Используется, если enable_gestures = true) ---
    // =============================================================================
    
    /**
     * @brief Синхронное перемещение (Используется строго внутри задач жестов).
     * Блокирует текущий поток, пока мотор не доедет до нужной точки.
     */
    void SetAngleSync(int target_degree, int speed_ms = 15) {
        if (target_degree < 0) target_degree = 0;
        if (target_degree > SERVO_MAX_DEGREE) target_degree = SERVO_MAX_DEGREE;
        
        int step = (target_degree > current_angle_) ? 1 : -1;
        while (current_angle_ != target_degree) {
            current_angle_ += step; 
            uint32_t duty = degree_to_duty(current_angle_);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
            if (speed_ms > 0) vTaskDelay(pdMS_TO_TICKS(speed_ms));
        }
    }

    /**
     * @brief Общая фоновая задача для проигрывания сложных жестов.
     */
    static void GestureTaskWrapper(void* pvParameters) {
        auto* data = static_cast<std::pair<ServoController*, int>*>(pvParameters);
        ServoController* servo = data->first;
        int gesture = data->second;
        int start_pos = servo->current_angle_;

        if (gesture == 1) { // Кивок (Nod)
            servo->SetAngleSync(120, 10); 
            servo->SetAngleSync(60, 10);  
            servo->SetAngleSync(start_pos, 15);
        } else if (gesture == 2) { // Отрицание (Shake)
            servo->SetAngleSync(start_pos + 30, 8); 
            servo->SetAngleSync(start_pos - 30, 8);  
            servo->SetAngleSync(start_pos, 12);
        } else if (gesture == 3) { // Приветствие (Wiggle)
            servo->SetAngleSync(130, 5); 
            servo->SetAngleSync(50, 5);  
            servo->SetAngleSync(130, 5); 
            servo->SetAngleSync(start_pos, 10);
        }

        delete data;
        vTaskDelete(NULL);
    }

    // Вспомогательные методы запуска жестов
    void Nod() {
        auto* data = new std::pair<ServoController*, int>(this, 1);
        if (xTaskCreate(GestureTaskWrapper, "srv_nod", 3072, data, 5, NULL) != pdPASS) delete data;
    }
    void Shake() {
        auto* data = new std::pair<ServoController*, int>(this, 2);
        if (xTaskCreate(GestureTaskWrapper, "srv_shk", 3072, data, 5, NULL) != pdPASS) delete data;
    }
    void Wiggle() {
        auto* data = new std::pair<ServoController*, int>(this, 3);
        if (xTaskCreate(GestureTaskWrapper, "srv_wgl", 3072, data, 5, NULL) != pdPASS) delete data;
    }

private:
    /**
     * @brief Регистрирует доступные команды в MCP-сервере для управления ИИ.
     */
    void RegisterMcpTool() {
        auto& mcp = McpServer::GetInstance();
        std::string base_cmd = "self.servo." + device_id_; // Базовая команда, напр. "self.servo.box_lid"
        
        // 1. БАЗОВЫЙ ИНСТРУМЕНТ (Доступен всегда)
        // Установка произвольного угла
        mcp.AddTool(base_cmd + ".set_angle", 
            "Установить '" + device_name_ + "' на заданный угол (0-180 градусов).", 
            PropertyList({
                Property("angle", kPropertyTypeInteger, 0, SERVO_MAX_DEGREE) 
            }), 
            [this](const PropertyList& params) -> ReturnValue {
                this->SetAngle(params["angle"].value<int>(), 15);
                return true; 
            });

        // Запрос текущего состояния
        mcp.AddTool(base_cmd + ".get_state",
            "Запросить текущий угол устройства '" + device_name_ + "'",
            PropertyList(),
            [this](const PropertyList& params) -> ReturnValue {
                this->SetAngle(params["angle"].value<int>(), 15);
                return "Текущий угол '" + device_name_ + "' составляет: " + std::to_string(this->current_angle_) + " градусов";
            });

        // 2. ИНСТРУМЕНТЫ АНИМАЦИИ (Регистрируются только если enable_gestures_ == true)
        if (enable_gestures_) {
            mcp.AddTool(base_cmd + ".wiggle", 
                "Выполнить жест приветствия устройством '" + device_name_ + "'", 
                PropertyList(), 
                [this](const PropertyList& params) -> ReturnValue {
                    this->Wiggle();
                    return "Запущена анимация приветствия для устройства '" + device_name_ + "'";
                });

            mcp.AddTool(base_cmd + ".nod", 
                "Кивнуть устройством '" + device_name_ + "' (выражение согласия/Да).", 
                PropertyList(), 
                [this](const PropertyList& params) -> ReturnValue {
                    this->Nod();
                    return device_name_ + " начинает кивать.";
                });

            mcp.AddTool(base_cmd + ".shake", 
                "Помотать устройством '" + device_name_ + "' (выражение отказа/Нет).", 
                PropertyList(), 
                [this](const PropertyList& params) -> ReturnValue {
                    this->Shake();
                    return device_name_ + " имитирует жест отрицания.";
                });
        }
    }
};

#endif // __SERVO_CONTROLLER_H__