#pragma once

#include <Types.hpp>
#include <string>
#include <vector>

namespace seabattle {

class GameRules {
   public:
    struct ValidationResult {
        bool valid;
        std::string error;
    };

    static ValidationResult validatePlacement(const std::vector<ShipPlacement>& ships);

    static bool isValidNickname(const std::string& nickname);

    static std::vector<int> getRequiredShipSizes();

   private:
    static bool checkShipCounts(const std::vector<ShipPlacement>& ships);
    static bool checkBounds(const std::vector<ShipPlacement>& ships);
    static bool checkOverlapAndTouching(const std::vector<ShipPlacement>& ships);
};

}  // namespace seabattle