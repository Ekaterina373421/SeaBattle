cmake_minimum_required(VERSION 3.16)
project(SeaBattle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Собрать тесты" ON)
option(BUILD_SERVER "Собрать сервер" ON)
option(BUILD_CLIENT "Собрать клиент" OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(nlohmann_json 3.9 REQUIRED)
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

add_subdirectory(common)

add_definitions(-D_WIN32_WINNT=0x0601)

if(BUILD_SERVER)
    add_subdirectory(server)
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()