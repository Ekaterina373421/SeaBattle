#include "Ship.hpp"

namespace seabattle {

Ship::Ship(int x, int y, int size, bool horizontal) {
    cells_.reserve(size);
    for (int i = 0; i < size; ++i) {
        if (horizontal) {
            cells_.emplace_back(x + i, y);
        } else {
            cells_.emplace_back(x, y + i);
        }
    }
}

void Ship::hit(int x, int y) {
    if (containsCell(x, y)) {
        hits_.emplace(x, y);
    }
}

bool Ship::isSunk() const {
    return hits_.size() == cells_.size();
}

bool Ship::containsCell(int x, int y) const {
    for (const auto& cell : cells_) {
        if (cell.first == x && cell.second == y) {
            return true;
        }
    }
    return false;
}

}  // namespace seabattle
