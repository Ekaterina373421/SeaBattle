#include "Board.hpp"

#include <algorithm>
#include <random>

namespace seabattle {

Board::Board() {
    clear();
}

void Board::clear() {
    for (auto& row : cells_) {
        row.fill(CellState::Empty);
    }
    ships_.clear();
}

bool Board::isValidCoordinate(int x, int y) const {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE;
}

bool Board::hasAdjacentShip(int x, int y) const {
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            int nx = x + dx;
            int ny = y + dy;
            if (isValidCoordinate(nx, ny) && cells_[ny][nx] == CellState::Ship) {
                return true;
            }
        }
    }
    return false;
}

bool Board::canPlaceShip(int x, int y, int size, bool horizontal) const {
    for (int i = 0; i < size; ++i) {
        int cx = horizontal ? x + i : x;
        int cy = horizontal ? y : y + i;

        if (!isValidCoordinate(cx, cy)) {
            return false;
        }

        if (hasAdjacentShip(cx, cy)) {
            return false;
        }
    }
    return true;
}

bool Board::placeShip(int x, int y, int size, bool horizontal) {
    if (!canPlaceShip(x, y, size, horizontal)) {
        return false;
    }

    auto ship = std::make_unique<Ship>(x, y, size, horizontal);

    for (const auto& cell : ship->getCells()) {
        cells_[cell.second][cell.first] = CellState::Ship;
    }

    ships_.push_back(std::move(ship));
    return true;
}

bool Board::placeShip(const ShipPlacement& placement) {
    return placeShip(placement.x, placement.y, placement.size, placement.horizontal);
}

ShotResult Board::shoot(int x, int y) {
    if (!isValidCoordinate(x, y)) {
        return ShotResult::InvalidCoordinates;
    }

    CellState& cell = cells_[y][x];

    if (cell == CellState::Hit || cell == CellState::Miss) {
        return ShotResult::AlreadyShot;
    }

    if (cell == CellState::Empty) {
        cell = CellState::Miss;
        return ShotResult::Miss;
    }

    cell = CellState::Hit;

    for (auto& ship : ships_) {
        if (ship->containsCell(x, y)) {
            ship->hit(x, y);
            if (ship->isSunk()) {
                markSurroundingAsMiss(*ship);
                return ShotResult::Kill;
            }
            return ShotResult::Hit;
        }
    }

    return ShotResult::Hit;
}

void Board::markSurroundingAsMiss(const Ship& ship) {
    auto surrounding = getSurroundingCells(ship);
    for (const auto& cell : surrounding) {
        if (cells_[cell.second][cell.first] == CellState::Empty) {
            cells_[cell.second][cell.first] = CellState::Miss;
        }
    }
}

std::vector<std::pair<int, int>> Board::getSurroundingCells(const Ship& ship) const {
    std::set<std::pair<int, int>> surrounding;

    for (const auto& cell : ship.getCells()) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0)
                    continue;

                int nx = cell.first + dx;
                int ny = cell.second + dy;

                if (isValidCoordinate(nx, ny) && !ship.containsCell(nx, ny)) {
                    surrounding.emplace(nx, ny);
                }
            }
        }
    }

    return {surrounding.begin(), surrounding.end()};
}

bool Board::areAllShipsSunk() const {
    for (const auto& ship : ships_) {
        if (!ship->isSunk()) {
            return false;
        }
    }
    return !ships_.empty();
}

CellState Board::getCell(int x, int y) const {
    if (!isValidCoordinate(x, y)) {
        return CellState::Empty;
    }
    return cells_[y][x];
}

const Ship* Board::getShipAt(int x, int y) const {
    for (const auto& ship : ships_) {
        if (ship->containsCell(x, y)) {
            return ship.get();
        }
    }
    return nullptr;
}

std::vector<CellInfo> Board::getFogOfWar() const {
    std::vector<CellInfo> result;

    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            CellState state = cells_[y][x];
            if (state == CellState::Hit || state == CellState::Miss) {
                result.push_back({x, y, state});
            }
        }
    }

    return result;
}

std::vector<CellInfo> Board::getFullState() const {
    std::vector<CellInfo> result;
    result.reserve(SIZE * SIZE);

    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            result.push_back({x, y, cells_[y][x]});
        }
    }

    return result;
}

void Board::placeShipsRandomly() {
    clear();

    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<int> shipSizes;
    for (int i = 0; i < GameConfig::SHIP_4_COUNT; ++i)
        shipSizes.push_back(4);
    for (int i = 0; i < GameConfig::SHIP_3_COUNT; ++i)
        shipSizes.push_back(3);
    for (int i = 0; i < GameConfig::SHIP_2_COUNT; ++i)
        shipSizes.push_back(2);
    for (int i = 0; i < GameConfig::SHIP_1_COUNT; ++i)
        shipSizes.push_back(1);

    for (int size : shipSizes) {
        bool placed = false;
        int attempts = 0;
        const int maxAttempts = 1000;

        while (!placed && attempts < maxAttempts) {
            std::uniform_int_distribution<> disX(0, SIZE - 1);
            std::uniform_int_distribution<> disY(0, SIZE - 1);
            std::uniform_int_distribution<> disDir(0, 1);

            int x = disX(gen);
            int y = disY(gen);
            bool horizontal = disDir(gen) == 0;

            if (canPlaceShip(x, y, size, horizontal)) {
                placeShip(x, y, size, horizontal);
                placed = true;
            }
            ++attempts;
        }

        if (!placed) {
            clear();
            placeShipsRandomly();
            return;
        }
    }
}

}  // namespace seabattle