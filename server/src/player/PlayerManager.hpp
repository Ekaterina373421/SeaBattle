#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Player.hpp"

namespace seabattle {

class PlayerManager {
   public:
    PlayerManager() = default;

    std::shared_ptr<Player> createPlayer(const std::string& nickname);
    std::shared_ptr<Player> getPlayer(const std::string& guid);
    std::shared_ptr<Player> getPlayerByNickname(const std::string& nickname);

    bool playerExists(const std::string& guid) const;

    bool setPlayerOnline(const std::string& guid, std::shared_ptr<Session> session);
    void setPlayerOffline(const std::string& guid);

    std::vector<PlayerInfo> getOnlinePlayers() const;
    std::vector<PlayerInfo> getAllPlayers() const;

    size_t getPlayerCount() const;
    size_t getOnlineCount() const;

   private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Player>> players_;
};

}  // namespace seabattle