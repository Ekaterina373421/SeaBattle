#pragma once

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace seabattle {

class Session;
class Player;

class SessionManager {
   public:
    using SessionPtr = std::shared_ptr<Session>;
    using DisconnectCallback = std::function<void(const std::string& playerGuid)>;

    SessionManager() = default;

    void addSession(SessionPtr session);
    void removeSession(SessionPtr session);
    void removeSession(uint64_t sessionId);

    SessionPtr getSession(uint64_t sessionId);
    SessionPtr getSessionByPlayerGuid(const std::string& playerGuid);

    void bindPlayerToSession(const std::string& playerGuid, SessionPtr session);
    void unbindPlayer(const std::string& playerGuid);

    void broadcast(const std::string& message);
    void broadcastExcept(const std::string& message, uint64_t excludeSessionId);
    void broadcastToPlayers(const std::string& message,
                            const std::vector<std::string>& playerGuids);

    void setDisconnectCallback(DisconnectCallback callback);

    void closeAll();

    size_t getSessionCount() const;
    size_t getAuthenticatedCount() const;

   private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, SessionPtr> sessions_;
    std::unordered_map<std::string, uint64_t> player_to_session_;
    DisconnectCallback disconnect_callback_;
    uint64_t next_session_id_ = 1;
};

}  // namespace seabattle