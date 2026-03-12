#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace seabattle {

class Server;
class Session;
class Game;

class MessageHandler {
   public:
    explicit MessageHandler(Server& server);

    void handleMessage(std::shared_ptr<Session> session, const std::string& rawMessage);

   private:
    void handleAuth(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleGetPlayers(std::shared_ptr<Session> session);
    void handleAddFriend(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleRemoveFriend(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleGetFriends(std::shared_ptr<Session> session);
    void handleGetStats(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleFindGame(std::shared_ptr<Session> session);
    void handleCancelSearch(std::shared_ptr<Session> session);
    void handleInvite(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleInviteResponse(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleSpectate(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handlePlaceShips(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleShoot(std::shared_ptr<Session> session, const nlohmann::json& payload);
    void handleSurrender(std::shared_ptr<Session> session);

    void sendError(std::shared_ptr<Session> session, int errorCode);
    void sendToPlayer(const std::string& playerGuid, const std::string& message);
    void notifyGameStart(std::shared_ptr<Game> game);
    void notifyBattleStart(std::shared_ptr<Game> game);
    void notifyGameOver(std::shared_ptr<Game> game);
    void checkMatchmaking();

    Server& server_;
};

}  // namespace seabattle