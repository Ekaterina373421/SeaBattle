#pragma once

#include <set>
#include <utility>
#include <vector>

namespace seabattle {

class Ship {
   public:
    Ship(int x, int y, int size, bool horizontal);

    void hit(int x, int y);
    bool isSunk() const;
    bool containsCell(int x, int y) const;

    const std::vector<std::pair<int, int>>& getCells() const {
        return cells_;
    }
    int getSize() const {
        return static_cast<int>(cells_.size());
    }
    int getHitCount() const {
        return static_cast<int>(hits_.size());
    }

   private:
    std::vector<std::pair<int, int>> cells_;
    std::set<std::pair<int, int>> hits_;
};

}  // namespace seabattle