#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "FriendList.hpp"
#include "IPlayer.hpp"
#include "PlayerStats.hpp"

namespace seabattle {

class Session;

class Player : public IPlayer {
   public:
    Player(const std::string& guid, const std::string& nickname);

    std::string getGuid() const override;
    std::string getNickname() const override;
    PlayerStatus getStatus() const override;
    void setStatus(PlayerStatus status) override;
    std::optional<std::string> getCurrentGameId() const override;
    void setCurrentGameId(const std::optional<std::string>& gameId) override;
    bool isOnline() const override;

    void setNickname(const std::string& nickname);

    std::weak_ptr<Session> getSession() const;
    void setSession(std::shared_ptr<Session> session);

    FriendList& getFriends();
    const FriendList& getFriends() const;

    PlayerStats& getStats();
    const PlayerStats& getStats() const;

    PlayerInfo toPlayerInfo() const;

   private:
    mutable std::mutex mutex_;
    std::string guid_;
    std::string nickname_;
    PlayerStatus status_ = PlayerStatus::Offline;
    std::optional<std::string> current_game_id_;
    std::weak_ptr<Session> session_;
    FriendList friends_;
    PlayerStats stats_;
};

}  // namespace seabattle