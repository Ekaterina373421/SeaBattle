#include "GameRules.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace seabattle {

std::vector<int> GameRules::getRequiredShipSizes() {
    std::vector<int> sizes;
    for (int i = 0; i < GameConfig::SHIP_4_COUNT; ++i)
        sizes.push_back(4);
    for (int i = 0; i < GameConfig::SHIP_3_COUNT; ++i)
        sizes.push_back(3);
    for (int i = 0; i < GameConfig::SHIP_2_COUNT; ++i)
        sizes.push_back(2);
    for (int i = 0; i < GameConfig::SHIP_1_COUNT; ++i)
        sizes.push_back(1);
    return sizes;
}

bool GameRules::checkShipCounts(const std::vector<ShipPlacement>& ships) {
    std::map<int, int> counts;
    for (const auto& ship : ships) {
        counts[ship.size]++;
    }

    return counts[4] == GameConfig::SHIP_4_COUNT && counts[3] == GameConfig::SHIP_3_COUNT &&
           counts[2] == GameConfig::SHIP_2_COUNT && counts[1] == GameConfig::SHIP_1_COUNT;
}

bool GameRules::checkBounds(const std::vector<ShipPlacement>& ships) {
    for (const auto& ship : ships) {
        if (ship.x < 0 || ship.y < 0) {
            return false;
        }

        int endX = ship.horizontal ? ship.x + ship.size - 1 : ship.x;
        int endY = ship.horizontal ? ship.y : ship.y + ship.size - 1;

        if (endX >= GameConfig::BOARD_SIZE || endY >= GameConfig::BOARD_SIZE) {
            return false;
        }
    }
    return true;
}

bool GameRules::checkOverlapAndTouching(const std::vector<ShipPlacement>& ships) {
    std::set<std::pair<int, int>> occupiedCells;
    std::set<std::pair<int, int>> forbiddenCells;

    for (const auto& ship : ships) {
        std::vector<std::pair<int, int>> shipCells;

        for (int i = 0; i < ship.size; ++i) {
            int x = ship.horizontal ? ship.x + i : ship.x;
            int y = ship.horizontal ? ship.y : ship.y + i;
            shipCells.emplace_back(x, y);
        }

        for (const auto& cell : shipCells) {
            if (forbiddenCells.count(cell) > 0) {
                return false;
            }
        }

        for (const auto& cell : shipCells) {
            occupiedCells.insert(cell);

            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    forbiddenCells.emplace(cell.first + dx, cell.second + dy);
                }
            }
        }
    }

    return true;
}

GameRules::ValidationResult GameRules::validatePlacement(const std::vector<ShipPlacement>& ships) {
    if (ships.size() != static_cast<size_t>(GameConfig::TOTAL_SHIPS)) {
        return {false, "Неверное количество кораблей"};
    }

    if (!checkShipCounts(ships)) {
        return {false, "Неверное количество кораблей каждого типа"};
    }

    if (!checkBounds(ships)) {
        return {false, "Корабль выходит за границы поля"};
    }

    if (!checkOverlapAndTouching(ships)) {
        return {false, "Корабли пересекаются или касаются друг друга"};
    }

    return {true, ""};
}

namespace {

bool isLatinLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isUnderscore(char c) {
    return c == '_';
}

bool isValidUtf8Cyrillic(const std::string& str, size_t& pos) {
    if (pos + 1 >= str.size()) {
        return false;
    }

    auto c1 = static_cast<unsigned char>(str[pos]);
    auto c2 = static_cast<unsigned char>(str[pos + 1]);

    if (c1 == 0xD0) {
        if ((c2 >= 0x90 && c2 <= 0xBF) || c2 == 0x81) {
            pos += 2;
            return true;
        }
    } else if (c1 == 0xD1) {
        if ((c2 >= 0x80 && c2 <= 0x8F) || c2 == 0x91) {
            pos += 2;
            return true;
        }
    }

    return false;
}

}  // namespace

bool GameRules::isValidNickname(const std::string& nickname) {
    if (nickname.empty()) {
        return false;
    }

    size_t charCount = 0;
    size_t pos = 0;

    while (pos < nickname.size()) {
        char c = nickname[pos];

        if (isLatinLetter(c) || isDigit(c) || isUnderscore(c)) {
            ++pos;
            ++charCount;
        } else if (isValidUtf8Cyrillic(nickname, pos)) {
            ++charCount;
        } else {
            return false;
        }
    }

    return charCount >= 3 && charCount <= 15;
}

}  // namespace seabattle