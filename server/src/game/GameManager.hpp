#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Game.hpp"

namespace seabattle {

class Player;
class Session;

class GameManager {
   public:
    GameManager() = default;

    std::shared_ptr<Game> createGame(std::shared_ptr<Player> player1,
                                     std::shared_ptr<Player> player2);

    std::shared_ptr<Game> getGame(const std::string& gameId);
    std::shared_ptr<Game> getGameByPlayer(const std::string& playerGuid);

    void removeGame(const std::string& gameId);

    bool addSpectator(const std::string& gameId, std::weak_ptr<Session> session);
    void removeSpectator(const std::string& gameId, std::weak_ptr<Session> session);

    std::vector<std::string> getActiveGameIds() const;
    size_t getActiveGameCount() const;

   private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Game>> games_;
    std::unordered_map<std::string, std::string> player_to_game_;
};

}  // namespace seabattle