// #ifndef __WEATHER_CONTROLLER_H__
// #define __WEATHER_CONTROLLER_H__

// #include "mcp_server.h"
// #include <string>

// class WeatherController {
// public:
//     WeatherController() {
//         auto& mcp = McpServer::GetInstance();

//         // Описание параметров (оставляем пустым, чтобы ИИ просто вызывал функцию)
//         PropertyList weather_props;

//         mcp.AddTool("self.get_weather", 
//             "Узнать погоду. Пользователь находится в г. Каменское, Днепропетровская обл., Украина.", 
//             weather_props, 
//             [](const PropertyList& params) -> ReturnValue {
//                 // Мы не используем захват [this], так как нам не нужны переменные класса.
//                 // Просто возвращаем текст-подсказку для ИИ.
//                 return "Пользователь из г. Каменское, Украина. Найди и расскажи актуальную погоду для этого города.";
//             });
//     }
// };

// #endif // __WEATHER_CONTROLLER_H__








// #ifndef __WEATHER_CONTROLLER_H__
// #define __WEATHER_CONTROLLER_H__

// #include "mcp_server.h"
// #include <string>

// class WeatherController {
// public:
//     WeatherController() {
//         auto& mcp = McpServer::GetInstance();

//         // Описание параметров
//         PropertyList weather_props;

//         mcp.AddTool("self.get_weather", 
//             "Узнать погоду. ВНИМАНИЕ: Пользователь ВСЕГДА находится в г. Каменское, Днепропетровская обл., Украина. Если город не указан, ищи только для г. Каменское.", 
//             weather_props, 
//             [](const PropertyList& params) -> ReturnValue {
//                 // Возвращаем инструкцию, которую ИИ увидит после вызова
//                 return "Пользователь находится в г. Каменское, Украина. Найди данные именно для этого города и сообщи их. Если данные кажутся неверными, уточни время последнего обновления.";
//             });
//     }
// };

// #endif // __WEATHER_CONTROLLER_H__









#ifndef __WEATHER_CONTROLLER_H__
#define __WEATHER_CONTROLLER_H__

#include "mcp_server.h"
#include <string>

class WeatherController {
public:
    WeatherController() {
        auto& mcp = McpServer::GetInstance();

        // Мы передаем пустой список параметров PropertyList().
        // Это значит, что ИИ вызовет функцию просто так, без аргументов.
        // А всю "магию" с городами мы переложим на ответную инструкцию.
        
        mcp.AddTool("self.get_weather", 
            "Получить прогноз погоды. Вызывай, когда пользователь спрашивает о погоде в любом месте.", 
            PropertyList(), 
            [](const PropertyList& params) -> ReturnValue {
                // Возвращаем ИИ жесткую инструкцию-алгоритм
                return "ВНИМАНИЕ: Это системный запрос погоды. Твои действия:\n"
                       "1. Проанализируй последнее сообщение пользователя.\n"
                       "2. Если он назвал город (например 'погода в Киеве') — найди погоду для ЭТОГО города.\n"
                       "3. Если город не назван — определи местоположение пользователя по IP-адресу.\n"
                       "4. Если IP не определяется — используй по умолчанию: Каменское, Украина.\n"
                       "5. Сообщи пользователю текущую температуру, осадки и ветер.";
            });
    }
};

#endif // __WEATHER_CONTROLLER_H__