#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace seabattle {

enum class CellState { Empty, Ship, Hit, Miss };

enum class ShotResult { Miss, Hit, Kill, AlreadyShot, InvalidCoordinates };

enum class GameState { Placing, Battle, Finished };

enum class PlayerStatus { Offline, Online, InGame };

struct ShipPlacement {
    int x;
    int y;
    int size;
    bool horizontal;
};

struct PlayerInfo {
    std::string guid;
    std::string nickname;
    PlayerStatus status;
    std::optional<std::string> game_id;
};

struct FriendInfo {
    std::string guid;
    std::string nickname;
    PlayerStatus status;
};

struct CellInfo {
    int x;
    int y;
    CellState state;
};

struct Invite {
    std::string id;
    std::string from_guid;
    std::string to_guid;
    std::chrono::steady_clock::time_point created_at;

    static constexpr auto TIMEOUT = std::chrono::seconds(30);

    bool isExpired() const {
        return std::chrono::steady_clock::now() - created_at > TIMEOUT;
    }
};

struct ShotResultInfo {
    ShotResult result;
    int x;
    int y;
    std::optional<std::vector<std::pair<int, int>>> killed_ship_cells;
    std::optional<std::vector<std::pair<int, int>>> surrounding_misses;
    std::string next_turn_guid;
};

struct GameConfig {
    static constexpr int BOARD_SIZE = 10;
    static constexpr int SHIP_4_COUNT = 1;
    static constexpr int SHIP_3_COUNT = 2;
    static constexpr int SHIP_2_COUNT = 3;
    static constexpr int SHIP_1_COUNT = 4;
    static constexpr int TOTAL_SHIPS = SHIP_4_COUNT + SHIP_3_COUNT + SHIP_2_COUNT + SHIP_1_COUNT;
    static constexpr int TOTAL_SHIP_CELLS =
        4 * SHIP_4_COUNT + 3 * SHIP_3_COUNT + 2 * SHIP_2_COUNT + 1 * SHIP_1_COUNT;
};

struct TimingConfig {
    static constexpr auto PLACEMENT_TIMEOUT = std::chrono::seconds(90);
    static constexpr auto TURN_TIMEOUT = std::chrono::seconds(30);
    static constexpr auto INVITE_TIMEOUT = std::chrono::seconds(30);
    static constexpr auto RECONNECT_TIMEOUT = std::chrono::seconds(10);
};

}  // namespace seabattle