#pragma once

#include <Types.hpp>
#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "Ship.hpp"

namespace seabattle {

class Board {
   public:
    static constexpr int SIZE = GameConfig::BOARD_SIZE;

    Board();

    bool placeShip(int x, int y, int size, bool horizontal);
    bool placeShip(const ShipPlacement& placement);

    ShotResult shoot(int x, int y);

    bool areAllShipsSunk() const;

    CellState getCell(int x, int y) const;

    std::vector<CellInfo> getFogOfWar() const;

    std::vector<CellInfo> getFullState() const;

    void placeShipsRandomly();

    void clear();

    const Ship* getShipAt(int x, int y) const;

    std::vector<std::pair<int, int>> getSurroundingCells(const Ship& ship) const;

    int getShipCount() const {
        return static_cast<int>(ships_.size());
    }

    bool canPlaceShip(int x, int y, int size, bool horizontal) const;

   private:
    std::array<std::array<CellState, SIZE>, SIZE> cells_;
    std::vector<std::unique_ptr<Ship>> ships_;

    bool isValidCoordinate(int x, int y) const;
    bool hasAdjacentShip(int x, int y) const;
    void markSurroundingAsMiss(const Ship& ship);
};

}  // namespace seabattle