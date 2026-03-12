#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace seabattle {

class Player;

class Matchmaker {
   public:
    Matchmaker() = default;

    void addPlayer(std::shared_ptr<Player> player);
    void removePlayer(const std::string& playerGuid);

    std::optional<std::pair<std::shared_ptr<Player>, std::shared_ptr<Player>>> tryMatch();

    bool isInQueue(const std::string& playerGuid) const;
    size_t getQueueSize() const;

    void clear();

   private:
    mutable std::mutex mutex_;
    std::deque<std::weak_ptr<Player>> queue_;

    void cleanupExpired();
};

}  // namespace seabattle