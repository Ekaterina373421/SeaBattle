set(SERVER_SOURCES
    src/utils/GuidGenerator.cpp
    src/utils/Logger.cpp
    src/game/Ship.cpp
    src/game/Board.cpp
    src/game/GameRules.cpp
    src/game/Game.cpp
    src/game/GameManager.cpp
    src/player/PlayerStats.cpp
    src/player/FriendList.cpp
    src/player/Player.cpp
    src/player/PlayerManager.cpp
    src/network/Session.cpp
    src/network/SessionManager.cpp
    src/network/Server.cpp
    src/network/MessageHandler.cpp
    src/lobby/Matchmaker.cpp
    src/lobby/InviteManager.cpp
    src/lobby/Lobby.cpp
    src/protocol/MessageParser.cpp
    src/protocol/MessageBuilder.cpp
)

add_library(seabattle_server_lib STATIC ${SERVER_SOURCES})

target_include_directories(seabattle_server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(seabattle_server_lib PUBLIC
    seabattle_common
    Boost::system
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)

add_executable(seabattle_server src/main.cpp)

target_link_libraries(seabattle_server PRIVATE
    seabattle_server_lib
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()