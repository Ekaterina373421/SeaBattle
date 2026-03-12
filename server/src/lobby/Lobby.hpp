#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "InviteManager.hpp"
#include "Matchmaker.hpp"

namespace seabattle {

class Player;
class Game;
class GameManager;

class Lobby {
   public:
    using GameCreatedCallback = std::function<void(std::shared_ptr<Game>)>;

    explicit Lobby(std::shared_ptr<GameManager> gameManager);

    void addToQueue(std::shared_ptr<Player> player);
    void removeFromQueue(const std::string& playerGuid);
    bool isInQueue(const std::string& playerGuid) const;
    size_t getQueueSize() const;

    std::optional<std::shared_ptr<Game>> tryMatchPlayers();

    std::string sendInvite(const std::string& fromGuid, const std::string& toGuid);
    std::optional<std::shared_ptr<Game>> acceptInvite(const std::string& inviteId,
                                                      std::shared_ptr<Player> fromPlayer,
                                                      std::shared_ptr<Player> toPlayer);
    bool declineInvite(const std::string& inviteId);
    bool cancelInvite(const std::string& inviteId, const std::string& fromGuid);

    std::optional<GameInvite> getInvite(const std::string& inviteId) const;
    std::vector<GameInvite> getInvitesForPlayer(const std::string& playerGuid) const;
    std::vector<GameInvite> getInvitesFromPlayer(const std::string& playerGuid) const;
    bool hasActiveInvite(const std::string& fromGuid, const std::string& toGuid) const;

    void handlePlayerDisconnect(const std::string& playerGuid);

    void cleanupExpiredInvites();

    Matchmaker& getMatchmaker() {
        return matchmaker_;
    }
    InviteManager& getInviteManager() {
        return invite_manager_;
    }

   private:
    std::shared_ptr<GameManager> game_manager_;
    Matchmaker matchmaker_;
    InviteManager invite_manager_;
};

}  // namespace seabattle