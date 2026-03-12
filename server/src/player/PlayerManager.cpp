#include "PlayerManager.hpp"

#include "utils/GuidGenerator.hpp"

namespace seabattle {

std::shared_ptr<Player> PlayerManager::createPlayer(const std::string& nickname) {
    std::string guid = GuidGenerator::generate();

    auto player = std::make_shared<Player>(guid, nickname);

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        players_[guid] = player;
    }

    return player;
}

std::shared_ptr<Player> PlayerManager::getPlayer(const std::string& guid) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = players_.find(guid);
    if (it != players_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Player> PlayerManager::getPlayerByNickname(const std::string& nickname) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [guid, player] : players_) {
        if (player->getNickname() == nickname) {
            return player;
        }
    }
    return nullptr;
}

bool PlayerManager::playerExists(const std::string& guid) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return players_.count(guid) > 0;
}

bool PlayerManager::setPlayerOnline(const std::string& guid, std::shared_ptr<Session> session) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = players_.find(guid);
    if (it == players_.end()) {
        return false;
    }

    it->second->setSession(session);
    it->second->setStatus(PlayerStatus::Online);
    return true;
}

void PlayerManager::setPlayerOffline(const std::string& guid) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = players_.find(guid);
    if (it != players_.end()) {
        it->second->setSession(nullptr);
        it->second->setStatus(PlayerStatus::Offline);
        it->second->setCurrentGameId(std::nullopt);
    }
}

std::vector<PlayerInfo> PlayerManager::getOnlinePlayers() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<PlayerInfo> result;

    for (const auto& [guid, player] : players_) {
        if (player->isOnline()) {
            result.push_back(player->toPlayerInfo());
        }
    }

    return result;
}

std::vector<PlayerInfo> PlayerManager::getAllPlayers() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<PlayerInfo> result;
    result.reserve(players_.size());

    for (const auto& [guid, player] : players_) {
        result.push_back(player->toPlayerInfo());
    }

    return result;
}

size_t PlayerManager::getPlayerCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return players_.size();
}

size_t PlayerManager::getOnlineCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [guid, player] : players_) {
        if (player->isOnline()) {
            ++count;
        }
    }
    return count;
}

}  // namespace seabattle
