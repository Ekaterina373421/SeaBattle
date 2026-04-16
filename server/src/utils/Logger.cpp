#include "Logger.hpp"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace seabattle {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init(const std::string& name, spdlog::level::level_enum level) {
    if (logger_) {
        return;
    }

    // приёмник логов выводит сообщения в stdout с цветной подсветкой (_mt - потокобезопасная
    // версия)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(level);

    logger_ = std::make_shared<spdlog::logger>(name, console_sink);
    logger_->set_level(level);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    spdlog::register_logger(logger_);
    spdlog::set_default_logger(logger_);
}

void Logger::setLevel(spdlog::level::level_enum level) {
    if (logger_) {
        logger_->set_level(level);
    }
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    if (!logger_) {
        init();
    }
    return logger_;
}

}  // namespace seabattle
