#include <Protocol.hpp>
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

#include "network/MessageHandler.hpp"
#include "network/Server.hpp"
#include "utils/Logger.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
std::atomic<bool> g_running{true};
seabattle::Server* g_server = nullptr;
}  // namespace

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        g_running = false;
        if (g_server) {
            g_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
        if (g_server) {
            g_server->stop();
        }
    }
}
#endif

void printBanner() {
    std::cout << R"(
  ____             ____        _   _   _      
 / ___|  ___  __ _| __ )  __ _| |_| |_| | ___ 
 \___ \ / _ \/ _` |  _ \ / _` | __| __| |/ _ \
  ___) |  __/ (_| | |_) | (_| | |_| |_| |  __/
 |____/ \___|\__,_|____/ \__,_|\__|\__|_|\___|
                                              
         Сервер морского боя v1.0
)" << std::endl;
}

void printHelp(const char* programName) {
    std::cout << "Использование: " << programName << " [опции]\n\n"
              << "Опции:\n"
              << "  --port <порт>      Порт сервера (по умолчанию: 12345)\n"
              << "  --threads <число>  Количество потоков (по умолчанию: 4)\n"
              << "  --debug            Включить отладочный вывод\n"
              << "  --quiet            Минимальный вывод (только ошибки)\n"
              << "  --help             Показать эту справку\n"
              << std::endl;
}

struct ServerConfig {
    uint16_t port = seabattle::protocol::DEFAULT_PORT;
    size_t threads = seabattle::protocol::DEFAULT_THREAD_COUNT;
    spdlog::level::level_enum logLevel = spdlog::level::info;
};

bool parseArgs(int argc, char* argv[], ServerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--port") {
            if (i + 1 >= argc) {
                std::cerr << "Ошибка: --port требует аргумент\n";
                return false;
            }
            try {
                int port = std::stoi(argv[++i]);
                if (port < 1 || port > 65535) {
                    std::cerr << "Ошибка: порт должен быть в диапазоне 1-65535\n";
                    return false;
                }
                config.port = static_cast<uint16_t>(port);
            } catch (...) {
                std::cerr << "Ошибка: неверный формат порта\n";
                return false;
            }
        } else if (arg == "--threads") {
            if (i + 1 >= argc) {
                std::cerr << "Ошибка: --threads требует аргумент\n";
                return false;
            }
            try {
                int threads = std::stoi(argv[++i]);
                if (threads < 1 || threads > 64) {
                    std::cerr << "Ошибка: количество потоков должно быть в диапазоне 1-64\n";
                    return false;
                }
                config.threads = static_cast<size_t>(threads);
            } catch (...) {
                std::cerr << "Ошибка: неверный формат количества потоков\n";
                return false;
            }
        } else if (arg == "--debug") {
            config.logLevel = spdlog::level::debug;
        } else if (arg == "--quiet") {
            config.logLevel = spdlog::level::err;
        } else if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Неизвестная опция: " << arg << "\n";
            printHelp(argv[0]);
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
    SetConsoleOutputCP(CP_UTF8);
#else
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#endif
    ServerConfig config;

    if (!parseArgs(argc, argv, config)) {
        return 1;
    }

    seabattle::Logger::init("seabattle", config.logLevel);

    if (config.logLevel != spdlog::level::err) {
        printBanner();
    }

    try {
        seabattle::Server server(config.port, config.threads);
        g_server = &server;

        auto handler = std::make_shared<seabattle::MessageHandler>(server);
        server.setMessageHandler(
            [handler](std::shared_ptr<seabattle::Session> session, const std::string& message) {
                handler->handleMessage(session, message);
            });

        server.run();

        seabattle::Logger::info("Сервер запущен на порту {} с {} потоками", config.port,
                                config.threads);
        seabattle::Logger::info("Нажмите Ctrl+C для остановки");

        while (g_running && server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        g_server = nullptr;
        server.stop();

        seabattle::Logger::info("Сервер завершил работу");

    } catch (const boost::system::system_error& e) {
        seabattle::Logger::critical("Ошибка сети: {}", e.what());
        if (e.code() == boost::asio::error::address_in_use) {
            seabattle::Logger::critical("Порт {} уже используется", config.port);
        }
        return 1;
    } catch (const std::exception& e) {
        seabattle::Logger::critical("Критическая ошибка: {}", e.what());
        return 1;
    }

    return 0;
}