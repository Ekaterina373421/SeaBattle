#pragma once

#include <Types.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace seabattle {

class Player;

struct GameInvite {
    std::string id;
    std::string from_guid;
    std::string to_guid;
    std::chrono::steady_clock::time_point created_at;

    bool isExpired() const {
        return std::chrono::steady_clock::now() - created_at > TimingConfig::INVITE_TIMEOUT;
    }
};

class InviteManager {
   public:
    InviteManager() = default;

    std::string createInvite(const std::string& fromGuid, const std::string& toGuid);

    std::optional<GameInvite> getInvite(const std::string& inviteId) const;

    std::optional<std::pair<std::string, std::string>> acceptInvite(const std::string& inviteId);

    bool declineInvite(const std::string& inviteId);

    bool cancelInvite(const std::string& inviteId, const std::string& fromGuid);

    std::vector<GameInvite> getInvitesForPlayer(const std::string& playerGuid) const;
    std::vector<GameInvite> getInvitesFromPlayer(const std::string& playerGuid) const;

    void removeInvitesForPlayer(const std::string& playerGuid);

    void cleanupExpired();

    size_t getInviteCount() const;

    bool hasActiveInvite(const std::string& fromGuid, const std::string& toGuid) const;

   private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, GameInvite> invites_;
};

}  // namespace seabattle