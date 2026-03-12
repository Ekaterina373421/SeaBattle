#include "Lobby.hpp"

#include "game/GameManager.hpp"
#include "player/Player.hpp"

namespace seabattle {

Lobby::Lobby(std::shared_ptr<GameManager> gameManager) : game_manager_(std::move(gameManager)) {}

void Lobby::addToQueue(std::shared_ptr<Player> player) {
    if (player && player->getStatus() == PlayerStatus::Online) {
        matchmaker_.addPlayer(player);
    }
}

void Lobby::removeFromQueue(const std::string& playerGuid) {
    matchmaker_.removePlayer(playerGuid);
}

bool Lobby::isInQueue(const std::string& playerGuid) const {
    return matchmaker_.isInQueue(playerGuid);
}

size_t Lobby::getQueueSize() const {
    return matchmaker_.getQueueSize();
}

std::optional<std::shared_ptr<Game>> Lobby::tryMatchPlayers() {
    auto match = matchmaker_.tryMatch();
    if (!match.has_value()) {
        return std::nullopt;
    }

    auto& [player1, player2] = *match;

    if (player1->getStatus() != PlayerStatus::Online ||
        player2->getStatus() != PlayerStatus::Online) {
        return std::nullopt;
    }

    auto game = game_manager_->createGame(player1, player2);
    return game;
}

std::string Lobby::sendInvite(const std::string& fromGuid, const std::string& toGuid) {
    return invite_manager_.createInvite(fromGuid, toGuid);
}

std::optional<std::shared_ptr<Game>> Lobby::acceptInvite(const std::string& inviteId,
                                                         std::shared_ptr<Player> fromPlayer,
                                                         std::shared_ptr<Player> toPlayer) {
    auto result = invite_manager_.acceptInvite(inviteId);
    if (!result.has_value()) {
        return std::nullopt;
    }

    if (!fromPlayer || !toPlayer) {
        return std::nullopt;
    }

    if (fromPlayer->getStatus() != PlayerStatus::Online ||
        toPlayer->getStatus() != PlayerStatus::Online) {
        return std::nullopt;
    }

    matchmaker_.removePlayer(fromPlayer->getGuid());
    matchmaker_.removePlayer(toPlayer->getGuid());

    auto game = game_manager_->createGame(fromPlayer, toPlayer);
    return game;
}

bool Lobby::declineInvite(const std::string& inviteId) {
    return invite_manager_.declineInvite(inviteId);
}

bool Lobby::cancelInvite(const std::string& inviteId, const std::string& fromGuid) {
    return invite_manager_.cancelInvite(inviteId, fromGuid);
}

std::optional<GameInvite> Lobby::getInvite(const std::string& inviteId) const {
    return invite_manager_.getInvite(inviteId);
}

std::vector<GameInvite> Lobby::getInvitesForPlayer(const std::string& playerGuid) const {
    return invite_manager_.getInvitesForPlayer(playerGuid);
}

std::vector<GameInvite> Lobby::getInvitesFromPlayer(const std::string& playerGuid) const {
    return invite_manager_.getInvitesFromPlayer(playerGuid);
}

bool Lobby::hasActiveInvite(const std::string& fromGuid, const std::string& toGuid) const {
    return invite_manager_.hasActiveInvite(fromGuid, toGuid);
}

void Lobby::handlePlayerDisconnect(const std::string& playerGuid) {
    matchmaker_.removePlayer(playerGuid);
    invite_manager_.removeInvitesForPlayer(playerGuid);
}

void Lobby::cleanupExpiredInvites() {
    invite_manager_.cleanupExpired();
}

}  // namespace seabattle
