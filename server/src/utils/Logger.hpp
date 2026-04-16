#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace seabattle {

class Logger {
   public:
    static void init(const std::string& name = "seabattle",
                     spdlog::level::level_enum level =
                         spdlog::level::info);  // уровни ниже info (debug, trace) не выводятся

    static void setLevel(spdlog::level::level_enum level);

    static std::shared_ptr<spdlog::logger>& get();

    template <typename... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->trace(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->debug(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->error(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->critical(fmt, std::forward<Args>(args)...);
    }

   private:
    static std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace seabattle