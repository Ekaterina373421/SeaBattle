set(TEST_SOURCES
    game/ShipTest.cpp
    game/BoardTest.cpp
    game/GameRulesTest.cpp
    game/GameTest.cpp
    player/PlayerStatsTest.cpp
    player/FriendListTest.cpp
    player/PlayerManagerTest.cpp
    lobby/MatchmakerTest.cpp
    lobby/InviteManagerTest.cpp
    lobby/LobbyTest.cpp
    protocol/MessageParserTest.cpp
    protocol/MessageBuilderTest.cpp
    integration/IntegrationTest.cpp
)

add_executable(seabattle_server_tests ${TEST_SOURCES})

target_include_directories(seabattle_server_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(seabattle_server_tests PRIVATE
    seabattle_server_lib
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(seabattle_server_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)