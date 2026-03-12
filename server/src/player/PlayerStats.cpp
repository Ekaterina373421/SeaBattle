#include "PlayerStats.hpp"

namespace seabattle {

void PlayerStats::recordWin() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++wins_;
    ++games_played_;
}

void PlayerStats::recordLoss() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++losses_;
    ++games_played_;
}

void PlayerStats::recordShot(bool hit) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++shots_fired_;
    if (hit) {
        ++shots_hit_;
    }
}

int PlayerStats::getGamesPlayed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return games_played_;
}

int PlayerStats::getWins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wins_;
}

int PlayerStats::getLosses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return losses_;
}

int PlayerStats::getShotsFired() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shots_fired_;
}

int PlayerStats::getShotsHit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shots_hit_;
}

double PlayerStats::getWinrate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (games_played_ == 0) {
        return 0.0;
    }
    return (static_cast<double>(wins_) / games_played_) * 100.0;
}

double PlayerStats::getAccuracy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shots_fired_ == 0) {
        return 0.0;
    }
    return (static_cast<double>(shots_hit_) / shots_fired_) * 100.0;
}

nlohmann::json PlayerStats::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {
        {"games_played", games_played_},
        {"wins", wins_},
        {"losses", losses_},
        {"winrate", games_played_ > 0 ? (static_cast<double>(wins_) / games_played_) * 100.0 : 0.0},
        {"shots_fired", shots_fired_},
        {"shots_hit", shots_hit_},
        {"accuracy",
         shots_fired_ > 0 ? (static_cast<double>(shots_hit_) / shots_fired_) * 100.0 : 0.0}};
}

}  // namespace seabattle
