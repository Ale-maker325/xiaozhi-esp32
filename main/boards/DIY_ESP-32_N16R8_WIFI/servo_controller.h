/**
 * @file servo_controller.h
 * @brief Потокобезопасное асинхронное управление сервоприводами.
 * Обеспечивает плавное движение сервоприводов без блокировки основного цикла программы.
 * Интегрирован с MCP-сервером для управления со стороны ИИ-модели.
 *
 * ВАЖНО (было исправлено): раньше каждый сервопривод сам регистрировал себе набор
 * MCP-инструментов (до 7 штук с жестами). При 5 сервоприводах + 2 реле (см.
 * my_controller_gpio.h) это упирается в лимит количества MCP-инструментов, который
 * проверяет СЕРВЕР (не прошивка) — на практике лимит оказался 32 на устройство суммарно,
 * а превышение приводит к нехватке памяти и обрывам соединения. Поэтому теперь
 * ServoController — только "чертёж" одного привода, БЕЗ собственной регистрации
 * инструментов. Инструменты регистрирует ОДИН раз на ВСЕ приводы сразу функция
 * RegisterServoTools() внизу файла — они принимают id привода параметром, поэтому
 * сколько бы приводов ни было — инструментов на них всегда ровно 7.
 */

#ifndef __SERVO_CONTROLLER_H__
#define __SERVO_CONTROLLER_H__

#include <driver/ledc.h>
#include "mcp_server.h"
#include <esp_log.h>
#include <string>
#include <vector>
#include <cstdio>        // ← snprintf (для форматирования адреса чипа PCA9685 в get_pin_info)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"    // ← хранение пользовательского имени привода во флеш (NVS)
#include "board.h"       // ← Board::GetInstance().GetDisplay() для уведомления при переименовании
#include "application.h" // ← Application::GetInstance().Reboot() после переименования
#include "config.h"      // ← для PCA9685_OUTPUT и пинов новой I2C-шины (если включён режим PCA9685)

#ifdef PCA9685_OUTPUT
#include <driver/i2c_master.h>
#include "i2c_device.h"  // ← общий для проекта помощник I2C-устройств (WriteReg/ReadReg), уже
                          //    используют другие платы репозитория для своих чипов (Pca9557 и т.п.)
#endif

// Константы SERVO_MIN_PULSEWIDTH_US/SERVO_MAX_PULSEWIDTH_US/SERVO_MAX_DEGREE/SERVO_FREQ_HZ
// теперь в config.h (раздел 5) — это настройки, которые можно захотеть поменять, поэтому
// они должны быть в главном настроечном файле платы, а не зашиты здесь.

#ifdef PCA9685_OUTPUT
// =============================================================================
// --- ДРАЙВЕР ЧИПА PCA9685 (16-канальный I2C ШИМ-контроллер) ---
// Один экземпляр на весь физический чип — разделяется всеми сервоприводами платы
// (у чипа общая частота ШИМ на все 16 каналов, задаётся один раз при инициализации).
// Регистровая карта — стандартная, по даташиту PCA9685:
//   MODE1 (0x00), MODE2 (0x01), PRE_SCALE (0xFE, пишется только в режиме SLEEP),
//   LEDn_ON_L/H, LEDn_OFF_L/H — по 4 регистра на канал, начиная с 0x06 (LED0_ON_L).
// =============================================================================
class Pca9685Driver : public I2cDevice {
public:
    // freq_hz здесь — частота ШИМ на выходе (обычно 50 Гц для серво), а не скорость I2C-шины.
    // Скорость самой I2C-шины берём из PCA9685_I2C_FREQ_HZ (config.h) через третий параметр
    // I2cDevice — раньше этот параметр объявлялся в config.h, но никуда не передавался и
    // не влиял на реальную скорость (I2cDevice жёстко использовал свои 400 кГц).
    Pca9685Driver(i2c_master_bus_handle_t i2c_bus, uint8_t addr, int freq_hz)
        : I2cDevice(i2c_bus, addr, PCA9685_I2C_FREQ_HZ) {
        // 1. SLEEP=1 — обязателен перед записью PRE_SCALE (по даташиту)
        WriteReg(0x00 /* MODE1 */, 0x10 /* SLEEP */);

        // 2. Прескейлер под нужную частоту ШИМ (обычно 50 Гц, как и у LEDC-варианта)
        //    Формула из даташита: round(25 МГц / (4096 * freq)) - 1
        int prescale = static_cast<int>((25000000.0 / (4096.0 * freq_hz)) + 0.5) - 1;
        if (prescale < 3) prescale = 3; // минимально допустимое значение по даташиту
        WriteReg(0xFE /* PRE_SCALE */, static_cast<uint8_t>(prescale));

        // 3. Выходим из SLEEP, ждём >=500 мкс на стабилизацию встроенного генератора
        WriteReg(0x00 /* MODE1 */, 0x00);
        vTaskDelay(pdMS_TO_TICKS(2));

        // 4. Тотем-поль выход (а не открытый сток) — правильно раскачивает сигнальную линию
        WriteReg(0x01 /* MODE2 */, 0x04 /* OUTDRV */);

        ESP_LOGI("PCA9685", "Инициализирован: адрес 0x%02X, prescale=%d (%d Гц)", addr, prescale, freq_hz);
    }

    // Канал (0-15) включается всегда с начала периода (ON=0), выключается через
    // off_ticks тиков (0-4095 из 4096 на весь период) — стандартный приём для того,
    // чтобы менять только длительность импульса, не трогая фазу.
    void SetChannelDuty(uint8_t channel, uint16_t off_ticks) {
        if (channel > 15) return;
        if (off_ticks > 4095) off_ticks = 4095;
        uint8_t base = 0x06 + 4 * channel; // LEDn_ON_L и далее по порядку
        WriteReg(base + 0, 0x00);
        WriteReg(base + 1, 0x00);
        WriteReg(base + 2, static_cast<uint8_t>(off_ticks & 0xFF));
        WriteReg(base + 3, static_cast<uint8_t>((off_ticks >> 8) & 0x0F));
    }
};
#endif // PCA9685_OUTPUT

class ServoController {
private:
    // В режиме PCA9685_OUTPUT: pin_ хранит номер канала чипа (0-15, см. config.h), а
    // channel_ (LEDC-канал) просто не используется — оставлен, чтобы не менять сигнатуру
    // конструктора и вызовы в esp_diy_N16_R8.cc.
    gpio_num_t pin_;             // Физический пин микроконтроллера, к которому подключен сигнальный провод
    ledc_channel_t channel_;     // Аппаратный ШИМ-канал (LEDC_CHANNEL_0, LEDC_CHANNEL_1 и т.д.)
    std::string device_id_;      // Системный идентификатор (например, "head" или "box_lid")
    std::string device_name_;    // Человекопонятное имя для ИИ (например, "Голова" или "Крышка шкатулки")
    int current_angle_ = 90;     // Текущий угол (стартуем всегда с нейтрального положения 90 градусов)
    bool enable_gestures_;       // Флаг: разрешить ли регистрацию анимаций (кивки, махания) для этого привода
#ifdef PCA9685_OUTPUT
    Pca9685Driver* pca9685_ = nullptr; // общий на все приводы драйвер чипа (см. конструктор)
#endif

    /**
     * @brief Конвертирует угол (0-180) в значение скважности (Duty Cycle).
     * Формула рассчитывает длину импульса в микросекундах и переводит её в тики таймера.
     * Разрешение зависит от бэкенда: 13 бит (8192 тика) у LEDC, 12 бит (4096 тиков) у PCA9685.
     */
    uint32_t degree_to_duty(int degree) {
        uint32_t cal_pulsewidth = (SERVO_MIN_PULSEWIDTH_US +
            (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * degree) / SERVO_MAX_DEGREE));

        // Период при 50 Гц = 20000 мкс, независимо от бэкенда.
#ifdef PCA9685_OUTPUT
        return (uint32_t)((1 << 12) * cal_pulsewidth / 20000);
#else
        return (uint32_t)((1 << 13) * cal_pulsewidth / 20000);
#endif
    }

#ifdef PCA9685_OUTPUT
    // Отправить готовое значение duty на канал чипа PCA9685 (pin_ хранит номер канала).
    void WriteDutyPca9685(uint32_t duty) {
        pca9685_->SetChannelDuty(static_cast<uint8_t>(pin_), static_cast<uint16_t>(duty));
    }
#endif

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
#ifdef PCA9685_OUTPUT
            servo->WriteDutyPca9685(duty);
#else
            ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel_, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel_);
#endif

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
    ServoController(gpio_num_t pin, const std::string& id, const std::string& default_name,
                    ledc_channel_t channel, bool enable_gestures = false)
        : pin_(pin), channel_(channel), device_id_(id), device_name_(default_name), enable_gestures_(enable_gestures) {

        // Если пользователь уже переименовывал этот привод голосом раньше — подхватываем
        // сохранённое имя из NVS вместо имени по умолчанию (см. RegisterServoTools() внизу файла).
        Settings name_settings("device_names", false);
        device_name_ = name_settings.GetString(device_id_, default_name);

#ifdef PCA9685_OUTPUT
        // --- Вариант PCA9685: общий на все приводы чип, инициализируется один раз ---
        // (тот же приём "static-флаг", что и у LEDC-таймера ниже, только тут ещё нужно
        // создать I2C-шину и сам объект драйвера — храним их тоже в static, чтобы дожили
        // до конца работы устройства и не пересоздавались на каждый сервопривод).
        static Pca9685Driver* driver = nullptr;
        if (driver == nullptr) {
            i2c_master_bus_config_t bus_config = {
                .i2c_port = PCA9685_I2C_NUM,
                .sda_io_num = PCA9685_I2C_SDA_PIN,
                .scl_io_num = PCA9685_I2C_SCL_PIN,
                .clk_source = I2C_CLK_SRC_DEFAULT,
                .glitch_ignore_cnt = 7,
                .intr_priority = 0,
                .trans_queue_depth = 0,
                .flags = { .enable_internal_pullup = 1 },
            };
            i2c_master_bus_handle_t bus;
            ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));
            driver = new Pca9685Driver(bus, PCA9685_I2C_ADDR, SERVO_FREQ_HZ);
            ESP_LOGI("Servo", "PCA9685: новая I2C-шина создана, драйвер готов.");
        }
        pca9685_ = driver;
        // pin_ в этом режиме — номер канала чипа (0-15), устанавливаем стартовый угол сразу
        WriteDutyPca9685(degree_to_duty(current_angle_));
#else
        // --- Вариант по умолчанию: прямой GPIO/LEDC ---
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
#endif

        ESP_LOGI("Servo", "Контроллер [%s] запущен. Пин/канал: %d, ШИМ-канал: %d, Анимации: %s",
                 device_name_.c_str(), pin_, channel_, enable_gestures_ ? "ВКЛ" : "ВЫКЛ");
        // Регистрация MCP-инструментов теперь общая на все приводы сразу —
        // см. RegisterServoTools() внизу файла, вызывается один раз из InitializeTools().
    }

    // --- Геттеры для общего RegisterServoTools() (см. внизу файла) ---
    const std::string& id() const { return device_id_; }
    const std::string& name() const { return device_name_; }
    int current_angle() const { return current_angle_; }
    bool gestures_enabled() const { return enable_gestures_; }
    gpio_num_t pin() const { return pin_; }
    ledc_channel_t channel() const { return channel_; }

    // Переименование "на лету". Сохраняет в NVS — попадёт в описания общих MCP-инструментов
    // после перезагрузки (её делает сам инструмент rename в RegisterServoTools()), а
    // device_name_ обновляем сразу, чтобы ответ в этом же разговоре был уже с новым именем.
    void Rename(const std::string& new_name) {
        Settings name_settings("device_names", true);
        name_settings.SetString(device_id_, new_name);
        device_name_ = new_name;
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
#ifdef PCA9685_OUTPUT
            WriteDutyPca9685(duty);
#else
            ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
#endif
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

};

/**
 * Регистрирует ОДИН общий набор из 7 MCP-инструментов на ВСЕ переданные сервоприводы —
 * вызывается один раз, после того как созданы все объекты ServoController.
 * Инструменты принимают параметр "id" и сами находят нужный привод в списке.
 */
inline void RegisterServoTools(std::vector<ServoController*> servos_in) {
    // static — список живёт всё время работы устройства, лямбды ниже безопасно держат
    // на него ссылку (servos_in — временный параметр, копируется в static один раз при
    // первом и единственном вызове этой функции).
    static std::vector<ServoController*> servos = servos_in;

    auto& mcp = McpServer::GetInstance();

    // id=Servo 1 ("сервопривод 1"); id=Servo 2 (...); ... — чтобы модель знала, какие id бывают.
    std::string servos_description;
    for (auto* s : servos) {
        if (!servos_description.empty()) servos_description += "; ";
        servos_description += "id=" + s->id() + " (\"" + s->name() + "\")";
    }

    mcp.AddTool("self.servo.set_angle",
        "Установить сервопривод по id на заданный угол (0-180 градусов). Доступные приводы: "
        + servos_description,
        PropertyList({
            Property("id", kPropertyTypeString, "id сервопривода"),
            Property("angle", kPropertyTypeInteger, 0, SERVO_MAX_DEGREE)
        }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() == id) {
                    s->SetAngle(p["angle"].value<int>(), 15);
                    return true;
                }
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.servo.get_state",
        "Запросить текущий угол сервопривода по id. Доступные приводы: " + servos_description,
        PropertyList({ Property("id", kPropertyTypeString, "id сервопривода") }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() == id) {
                    return "Текущий угол '" + s->name() + "' составляет: "
                        + std::to_string(s->current_angle()) + " градусов";
                }
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });

    // По умолчанию модель номер GPIO не знает и не упоминает — но если пользователь явно
    // спросит "на каком пине", у неё должна быть возможность узнать и ответить.
    mcp.AddTool("self.servo.get_pin_info",
#ifdef PCA9685_OUTPUT
        "Показать техническую информацию (номер канала чипа PCA9685) сервопривода по id. "
        "Вызывай ТОЛЬКО если пользователь явно спрашивает про пин/канал/техническую схему "
        "подключения — не для обычных команд поворота/переименования. Доступные приводы: "
        + servos_description,
#else
        "Показать техническую информацию (номер GPIO и канал ШИМ) сервопривода по id. Вызывай "
        "ТОЛЬКО если пользователь явно спрашивает про пин/GPIO/техническую схему подключения — "
        "не для обычных команд поворота/переименования. Доступные приводы: " + servos_description,
#endif
        PropertyList({ Property("id", kPropertyTypeString, "id сервопривода") }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() == id) {
#ifdef PCA9685_OUTPUT
                    char addr_hex[6];
                    snprintf(addr_hex, sizeof(addr_hex), "0x%02X", PCA9685_I2C_ADDR);
                    return "Привод '" + s->name() + "' (id: " + s->id()
                        + ") подключён к каналу " + std::to_string(static_cast<int>(s->pin()))
                        + " чипа PCA9685 (I2C-адрес " + addr_hex + ").";
#else
                    return "Привод '" + s->name() + "' (id: " + s->id() + ") подключён к GPIO"
                        + std::to_string(static_cast<int>(s->pin())) + ", ШИМ-канал "
                        + std::to_string(static_cast<int>(s->channel())) + ".";
#endif
                }
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.servo.rename",
        "Переименовать сервопривод по id (например: 'пусть Servo 2 будет рукой'). Доступные "
        "приводы: " + servos_description,
        PropertyList({
            Property("id", kPropertyTypeString, "id сервопривода"),
            Property("name", kPropertyTypeString, "Новое имя, например 'рука' или 'голова'")
        }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() != id) continue;

                std::string new_name = p["name"].value<std::string>();
                s->Rename(new_name);

                auto display = Board::GetInstance().GetDisplay();
                if (display) {
                    display->ShowNotification(("Переименовано: " + new_name).c_str(), 3000);
                }
                ESP_LOGI("Servo", "'%s' переименован в '%s', перезагрузка...", id.c_str(), new_name.c_str());

                vTaskDelay(pdMS_TO_TICKS(1000));
                Application::GetInstance().Reboot();
                return "Готово, теперь этот привод называется '" + new_name
                    + "'. Перезагружаюсь, чтобы новое имя вступило в силу.";
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });

    // Жесты регистрируем всегда одним общим инструментом на все приводы — если у конкретного
    // привода анимации не включены (enable_gestures == false), инструмент вежливо откажет,
    // не вызывая реального движения.
    mcp.AddTool("self.servo.wiggle",
        "Выполнить жест приветствия сервоприводом по id (если у него включены анимации). "
        "Доступные приводы: " + servos_description,
        PropertyList({ Property("id", kPropertyTypeString, "id сервопривода") }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() != id) continue;
                if (!s->gestures_enabled()) return "У привода '" + s->name() + "' анимации не включены.";
                s->Wiggle();
                return "Запущена анимация приветствия для '" + s->name() + "'";
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.servo.nod",
        "Кивнуть сервоприводом по id, выражение согласия/Да (если у него включены анимации). "
        "Доступные приводы: " + servos_description,
        PropertyList({ Property("id", kPropertyTypeString, "id сервопривода") }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() != id) continue;
                if (!s->gestures_enabled()) return "У привода '" + s->name() + "' анимации не включены.";
                s->Nod();
                return s->name() + " начинает кивать.";
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.servo.shake",
        "Помотать сервоприводом по id, выражение отказа/Нет (если у него включены анимации). "
        "Доступные приводы: " + servos_description,
        PropertyList({ Property("id", kPropertyTypeString, "id сервопривода") }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* s : servos) {
                if (s->id() != id) continue;
                if (!s->gestures_enabled()) return "У привода '" + s->name() + "' анимации не включены.";
                s->Shake();
                return s->name() + " имитирует жест отрицания.";
            }
            return std::string("Ошибка: сервопривод с id '") + id + "' не найден.";
        });
}

#endif // __SERVO_CONTROLLER_H__