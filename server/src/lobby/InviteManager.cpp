#include "InviteManager.hpp"

#include <algorithm>

#include "utils/GuidGenerator.hpp"

namespace seabattle {

std::string InviteManager::createInvite(const std::string& fromGuid, const std::string& toGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [id, invite] : invites_) {
        if (invite.from_guid == fromGuid && invite.to_guid == toGuid && !invite.isExpired()) {
            return "";
        }
    }

    std::string inviteId = GuidGenerator::generate();

    GameInvite invite;
    invite.id = inviteId;
    invite.from_guid = fromGuid;
    invite.to_guid = toGuid;
    invite.created_at = std::chrono::steady_clock::now();

    invites_[inviteId] = invite;

    return inviteId;
}

std::optional<GameInvite> InviteManager::getInvite(const std::string& inviteId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = invites_.find(inviteId);
    if (it != invites_.end() && !it->second.isExpired()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::pair<std::string, std::string>> InviteManager::acceptInvite(
    const std::string& inviteId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = invites_.find(inviteId);
    if (it == invites_.end()) {
        return std::nullopt;
    }

    if (it->second.isExpired()) {
        invites_.erase(it);
        return std::nullopt;
    }

    auto result = std::make_pair(it->second.from_guid, it->second.to_guid);
    invites_.erase(it);

    return result;
}

bool InviteManager::declineInvite(const std::string& inviteId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = invites_.find(inviteId);
    if (it != invites_.end()) {
        invites_.erase(it);
        return true;
    }
    return false;
}

bool InviteManager::cancelInvite(const std::string& inviteId, const std::string& fromGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = invites_.find(inviteId);
    if (it != invites_.end() && it->second.from_guid == fromGuid) {
        invites_.erase(it);
        return true;
    }
    return false;
}

std::vector<GameInvite> InviteManager::getInvitesForPlayer(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GameInvite> result;
    for (const auto& [id, invite] : invites_) {
        if (invite.to_guid == playerGuid && !invite.isExpired()) {
            result.push_back(invite);
        }
    }
    return result;
}

std::vector<GameInvite> InviteManager::getInvitesFromPlayer(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GameInvite> result;
    for (const auto& [id, invite] : invites_) {
        if (invite.from_guid == playerGuid && !invite.isExpired()) {
            result.push_back(invite);
        }
    }
    return result;
}

void InviteManager::removeInvitesForPlayer(const std::string& playerGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = invites_.begin(); it != invites_.end();) {
        if (it->second.from_guid == playerGuid || it->second.to_guid == playerGuid) {
            it = invites_.erase(it);
        } else {
            ++it;
        }
    }
}

void InviteManager::cleanupExpired() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = invites_.begin(); it != invites_.end();) {
        if (it->second.isExpired()) {
            it = invites_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t InviteManager::getInviteCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return invites_.size();
}

bool InviteManager::hasActiveInvite(const std::string& fromGuid, const std::string& toGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [id, invite] : invites_) {
        if (invite.from_guid == fromGuid && invite.to_guid == toGuid && !invite.isExpired()) {
            return true;
        }
    }
    return false;
}

}  // namespace seabattle
