#pragma once

#include <Types.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Board.hpp"
#include "GameRules.hpp"

namespace seabattle {

class Player;
class Session;

struct GamePlayer {
    std::string guid;
    std::string nickname;
    std::unique_ptr<Board> board;
    bool ready = false;
};

class Game {
   public:
    using SpectatorCallback = std::function<void(const std::string&)>;

    Game(const std::string& id, std::shared_ptr<Player> player1, std::shared_ptr<Player> player2);

    std::string getId() const;
    GameState getState() const;
    std::string getCurrentTurnGuid() const;

    std::string getPlayer1Guid() const;
    std::string getPlayer2Guid() const;
    std::string getPlayerNickname(const std::string& guid) const;
    std::string getOpponentGuid(const std::string& playerGuid) const;

    bool isPlayer(const std::string& guid) const;
    bool isPlayerTurn(const std::string& guid) const;
    bool isPlayerReady(const std::string& guid) const;
    bool areBothPlayersReady() const;

    bool placeShips(const std::string& playerGuid, const std::vector<ShipPlacement>& ships);
    void placeShipsRandomly(const std::string& playerGuid);

    ShotResultInfo shoot(const std::string& shooterGuid, int x, int y);

    void surrender(const std::string& playerGuid);
    void handleDisconnect(const std::string& playerGuid);

    void startBattle();
    bool isFinished() const;
    std::optional<std::string> getWinnerGuid() const;
    std::string getGameOverReason() const;

    void addSpectator(std::weak_ptr<Session> session);
    void removeSpectator(std::weak_ptr<Session> session);
    void notifySpectators(const std::string& message);
    size_t getSpectatorCount() const;

    std::vector<CellInfo> getBoardFogOfWar(const std::string& playerGuid) const;
    std::vector<CellInfo> getBoardFullState(const std::string& playerGuid) const;

    Board* getPlayerBoard(const std::string& playerGuid);
    const Board* getPlayerBoard(const std::string& playerGuid) const;

   private:
    mutable std::mutex mutex_;
    std::string id_;
    GameState state_ = GameState::Placing;
    std::string current_turn_guid_;
    std::optional<std::string> winner_guid_;
    std::string game_over_reason_;

    GamePlayer player1_;
    GamePlayer player2_;

    std::vector<std::weak_ptr<Session>> spectators_;

    GamePlayer& getGamePlayer(const std::string& guid);
    const GamePlayer& getGamePlayer(const std::string& guid) const;
    GamePlayer& getOpponentGamePlayer(const std::string& guid);

    void switchTurn();
    void finishGame(const std::string& winnerGuid, const std::string& reason);
    void cleanupSpectators();
};

}  // namespace seabattle