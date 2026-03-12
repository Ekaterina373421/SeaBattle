#pragma once

#include <mutex>
#include <nlohmann/json.hpp>

namespace seabattle {

class PlayerStats {
   public:
    PlayerStats() = default;

    void recordWin();
    void recordLoss();
    void recordShot(bool hit);

    int getGamesPlayed() const;
    int getWins() const;
    int getLosses() const;
    int getShotsFired() const;
    int getShotsHit() const;

    double getWinrate() const;
    double getAccuracy() const;

    nlohmann::json toJson() const;

   private:
    mutable std::mutex mutex_;
    int games_played_ = 0;
    int wins_ = 0;
    int losses_ = 0;
    int shots_fired_ = 0;
    int shots_hit_ = 0;
};

}  // namespace seabattle