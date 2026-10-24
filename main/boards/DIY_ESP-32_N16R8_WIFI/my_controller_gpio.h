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
#include <vector>
// Хранилище настроек во флеш-памяти (NVS) — тут храним пользовательские имена устройств,
// чтобы они переживали перезагрузку и переподключение.
#include "settings.h"
// Для Board::GetInstance().GetDisplay() (показать уведомление при переименовании)
#include "board.h"
// Для Application::GetInstance().Reboot() — перезагрузка после переименования, чтобы
// новое имя попало в описание MCP-инструмента (описание фиксируется один раз при старте).
#include "application.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * ВАЖНО (было исправлено): раньше каждый экземпляр Controller_GPIO сам регистрировал
 * себе 5 MCP-инструментов (get_state/turn_on/turn_off/get_pin_info/rename). При 2+ реле
 * и 5 сервоприводах (см. servo_controller.h) это быстро упирается в лимит количества
 * инструментов, который проверяет СЕРВЕР (а не прошивка) — на практике оказалось, что
 * лимит — 32 инструмента на устройство суммарно. Превышение не отклоняется локально
 * (AddTool просто добавляет в список), но сервер после этого присылает ошибку
 * "The number of tools has reached the limit: 32", и дальше устройство ведёт себя
 * нестабильно (нехватка памяти, обрывы соединения).
 *
 * Поэтому теперь класс Controller_GPIO — это только "чертёж" одного силового канала
 * (состояние + GPIO), БЕЗ собственной регистрации MCP-инструментов. Инструменты
 * регистрируются ОДИН раз на ВСЕ каналы сразу функцией RegisterRelayTools() ниже —
 * они принимают id канала параметром, поэтому сколько бы реле ни было (2, 3, 10) —
 * MCP-инструментов на них всегда ровно 5.
 */
class Controller_GPIO {
public:
    /**
     * КОНСТРУКТОР КЛАССА
     * Принимает: номер пина, ID (стабильный технический id для кода/NVS) и имя ПО
     * УМОЛЧАНИЮ (для людей/ИИ) — если пользователь раньше уже переименовал этот канал
     * голосом, реальное имя подхватится из флеш-памяти вместо этого.
     */
    Controller_GPIO(gpio_num_t gpio_num, const std::string& id, const std::string& default_name)
        : gpio_num_(gpio_num), device_id_(id) {

        // Если пользователь уже переименовывал этот канал раньше — подхватываем
        // сохранённое имя из NVS вместо имени по умолчанию.
        Settings name_settings("device_names", false);
        device_name_ = name_settings.GetString(device_id_, default_name);

        // Настройка GPIO на выход, низкий уровень при старте (чтобы ничего случайно
        // не включилось само при подаче питания на плату).
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(gpio_num_, 0);
    }

    void TurnOn() {
        power_ = true;
        gpio_set_level(gpio_num_, 1);
    }

    void TurnOff() {
        power_ = false;
        gpio_set_level(gpio_num_, 0);
    }

    bool IsOn() const { return power_; }
    gpio_num_t gpio() const { return gpio_num_; }
    const std::string& id() const { return device_id_; }
    const std::string& name() const { return device_name_; }

    // Переименование "на лету". Сохраняет новое имя в NVS — оно попадёт в описания
    // общих MCP-инструментов только после перезагрузки (её вызывает сам инструмент
    // rename в RegisterRelayTools(), см. ниже), а device_name_ обновляем сразу же,
    // чтобы текст ответа в этом же разговоре уже был с новым именем.
    void Rename(const std::string& new_name) {
        Settings name_settings("device_names", true);
        name_settings.SetString(device_id_, new_name);
        device_name_ = new_name;
    }

private:
    bool power_ = false;
    gpio_num_t gpio_num_;
    std::string device_id_;   // стабильный технический id, не меняется никогда
    std::string device_name_; // человекопонятное имя, можно переименовать голосом
};

/**
 * Регистрирует ОДИН общий набор из 5 MCP-инструментов на ВСЕ переданные силовые каналы —
 * вызывается один раз, после того как созданы все объекты Controller_GPIO.
 * Инструменты принимают параметр "id" и сами находят нужный канал в списке.
 */
inline void RegisterRelayTools(std::vector<Controller_GPIO*> relays_in) {
    // static — список живёт всё время работы устройства, лямбды ниже безопасно держат
    // на него ссылку (relays_in — временный параметр, копировать его в static нужно
    // один раз при первом и единственном вызове этой функции).
    static std::vector<Controller_GPIO*> relays = relays_in;

    auto& mcp = McpServer::GetInstance();

    // Собираем строку вида: id=relay_ch1 ("вывод управления реле номер один"); id=relay_ch2 (...)
    // Используется в описаниях инструментов, чтобы модель знала, какие id вообще бывают.
    std::string channels_description;
    for (auto* r : relays) {
        if (!channels_description.empty()) channels_description += "; ";
        channels_description += "id=" + r->id() + " (\"" + r->name() + "\")";
    }

    PropertyList id_only({ Property("id", kPropertyTypeString, "id силового канала") });

    mcp.AddTool("self.relay.get_state",
        "Узнать состояние силового канала по id. Доступные каналы: " + channels_description,
        id_only,
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* r : relays) {
                if (r->id() == id) {
                    return r->IsOn() ? "{\"power\": true}" : "{\"power\": false}";
                }
            }
            return std::string("Ошибка: силовой канал с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.relay.turn_on",
        "Включить силовой канал по id. Доступные каналы: " + channels_description,
        id_only,
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* r : relays) {
                if (r->id() == id) {
                    r->TurnOn();
                    return true;
                }
            }
            return std::string("Ошибка: силовой канал с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.relay.turn_off",
        "Выключить силовой канал по id. Доступные каналы: " + channels_description,
        id_only,
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* r : relays) {
                if (r->id() == id) {
                    r->TurnOff();
                    return true;
                }
            }
            return std::string("Ошибка: силовой канал с id '") + id + "' не найден.";
        });

    // По умолчанию модель номер GPIO не знает и не упоминает — но если пользователь явно
    // спросит "на каком пине", у неё должна быть возможность узнать и ответить.
    mcp.AddTool("self.relay.get_pin_info",
        "Показать техническую информацию (номер GPIO) силового канала по id. Вызывай ТОЛЬКО "
        "если пользователь явно спрашивает про пин/GPIO/техническую схему подключения — не "
        "для обычных команд включения/выключения/переименования. Доступные каналы: "
        + channels_description,
        id_only,
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* r : relays) {
                if (r->id() == id) {
                    return "Канал '" + r->name() + "' (id: " + r->id() + ") подключён к GPIO"
                        + std::to_string(static_cast<int>(r->gpio())) + ".";
                }
            }
            return std::string("Ошибка: силовой канал с id '") + id + "' не найден.";
        });

    mcp.AddTool("self.relay.rename",
        "Переименовать силовой канал по id — задать своё имя устройству, которое к нему "
        "физически подключено (например: 'пусть relay_ch1 будет чайником'). Доступные "
        "каналы: " + channels_description,
        PropertyList({
            Property("id", kPropertyTypeString, "id силового канала"),
            Property("name", kPropertyTypeString, "Новое имя, например 'чайник'")
        }),
        [](const PropertyList& p) -> ReturnValue {
            const std::string& id = p["id"].value<std::string>();
            for (auto* r : relays) {
                if (r->id() != id) continue;

                std::string new_name = p["name"].value<std::string>();
                r->Rename(new_name);

                auto display = Board::GetInstance().GetDisplay();
                if (display) {
                    display->ShowNotification(("Переименовано: " + new_name).c_str(), 3000);
                }
                ESP_LOGI("Controller_GPIO", "'%s' переименован в '%s', перезагрузка...",
                    id.c_str(), new_name.c_str());

                vTaskDelay(pdMS_TO_TICKS(1000));
                Application::GetInstance().Reboot();
                return "Готово, теперь этот канал называется '" + new_name
                    + "'. Перезагружаюсь, чтобы новое имя вступило в силу.";
            }
            return std::string("Ошибка: силовой канал с id '") + id + "' не найден.";
        });
}

#endif // Завершение блока __LAMP_CONTROLLER_H__
