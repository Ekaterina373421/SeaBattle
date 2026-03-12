#include "SessionManager.hpp"

#include "Session.hpp"
#include "player/Player.hpp"

namespace seabattle {

void SessionManager::addSession(SessionPtr session) {
    if (!session)
        return;

    std::unique_lock<std::shared_mutex> lock(mutex_);
    uint64_t id = next_session_id_++;
    session->setId(id);
    sessions_[id] = session;
}

void SessionManager::removeSession(SessionPtr session) {
    if (!session)
        return;
    removeSession(session->getId());
}

void SessionManager::removeSession(uint64_t sessionId) {
    std::string playerGuid;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = sessions_.find(sessionId);
        if (it == sessions_.end())
            return;

        auto session = it->second;
        if (auto player = session->getPlayer()) {
            playerGuid = player->getGuid();
            player_to_session_.erase(playerGuid);
        }

        sessions_.erase(it);
    }

    if (!playerGuid.empty() && disconnect_callback_) {
        disconnect_callback_(playerGuid);
    }
}

SessionManager::SessionPtr SessionManager::getSession(uint64_t sessionId) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

SessionManager::SessionPtr SessionManager::getSessionByPlayerGuid(const std::string& playerGuid) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = player_to_session_.find(playerGuid);
    if (it != player_to_session_.end()) {
        auto sessionIt = sessions_.find(it->second);
        if (sessionIt != sessions_.end()) {
            return sessionIt->second;
        }
    }
    return nullptr;
}

void SessionManager::bindPlayerToSession(const std::string& playerGuid, SessionPtr session) {
    if (!session || playerGuid.empty())
        return;

    std::unique_lock<std::shared_mutex> lock(mutex_);
    player_to_session_[playerGuid] = session->getId();
}

void SessionManager::unbindPlayer(const std::string& playerGuid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    player_to_session_.erase(playerGuid);
}

void SessionManager::broadcast(const std::string& message) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
        if (session->isOpen()) {
            session->send(message);
        }
    }
}

void SessionManager::broadcastExcept(const std::string& message, uint64_t excludeSessionId) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
        if (id != excludeSessionId && session->isOpen()) {
            session->send(message);
        }
    }
}

void SessionManager::broadcastToPlayers(const std::string& message,
                                        const std::vector<std::string>& playerGuids) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& guid : playerGuids) {
        auto it = player_to_session_.find(guid);
        if (it != player_to_session_.end()) {
            auto sessionIt = sessions_.find(it->second);
            if (sessionIt != sessions_.end() && sessionIt->second->isOpen()) {
                sessionIt->second->send(message);
            }
        }
    }
}

void SessionManager::setDisconnectCallback(DisconnectCallback callback) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    disconnect_callback_ = std::move(callback);
}

void SessionManager::closeAll() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
        session->close();
    }
    sessions_.clear();
    player_to_session_.clear();
}

size_t SessionManager::getSessionCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return sessions_.size();
}

size_t SessionManager::getAuthenticatedCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return player_to_session_.size();
}

}  // namespace seabattle
