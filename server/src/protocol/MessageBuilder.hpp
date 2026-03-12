#pragma once

#include <Protocol.hpp>
#include <Types.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace seabattle {

class PlayerStats;
struct GameInvite;

class MessageBuilder {
   public:
    static std::string buildAuthResponse(bool success, const std::string& guid,
                                         const std::string& nickname);

    static std::string buildAuthError(int errorCode, const std::string& message);

    static std::string buildPlayerList(const std::vector<PlayerInfo>& players);

    static std::string buildFriendList(const std::vector<FriendInfo>& friends);

    static std::string buildStatsResponse(const std::string& guid, const std::string& nickname,
                                          const PlayerStats& stats);

    static std::string buildPlayerStatusChanged(
        const std::string& guid, PlayerStatus status,
        const std::optional<std::string>& gameId = std::nullopt);

    static std::string buildInviteReceived(const std::string& inviteId, const std::string& fromGuid,
                                           const std::string& fromNickname);

    static std::string buildInviteResponse(bool success, const std::string& inviteId);

    static std::string buildGameStarted(const std::string& gameId, const std::string& opponentGuid,
                                        const std::string& opponentNickname);

    static std::string buildFindGameResponse(bool inQueue, size_t queueSize);

    static std::string buildPlaceShipsResponse(bool success, const std::string& error = "");

    static std::string buildPlayerReady(const std::string& playerGuid);

    static std::string buildBattleStart(const std::string& firstTurnGuid);

    static std::string buildShotResult(const ShotResultInfo& info);

    static std::string buildOpponentShot(
        int x, int y, ShotResult result, bool yourTurn,
        const std::optional<std::vector<std::pair<int, int>>>& killedCells = std::nullopt,
        const std::optional<std::vector<std::pair<int, int>>>& surroundingMisses = std::nullopt);

    static std::string buildGameOver(const std::string& winnerGuid, const std::string& reason,
                                     const std::vector<CellInfo>& board1,
                                     const std::vector<CellInfo>& board2);

    static std::string buildSpectateResponse(bool success, const std::string& player1Guid,
                                             const std::string& player1Nickname,
                                             const std::string& player2Guid,
                                             const std::string& player2Nickname,
                                             const std::string& currentTurn,
                                             const std::vector<CellInfo>& board1Fog,
                                             const std::vector<CellInfo>& board2Fog);

    static std::string buildSpectateUpdate(const std::string& shooterGuid, int x, int y,
                                           ShotResult result, const std::string& nextTurn);

    static std::string buildError(int errorCode, const std::string& message);

    static std::string buildSuccess(const std::string& action);

   private:
    static nlohmann::json createBase(const std::string& type, const std::string& action);
    static std::string shotResultToString(ShotResult result);
    static std::string playerStatusToString(PlayerStatus status);
    static nlohmann::json cellInfoToJson(const CellInfo& cell);
    static nlohmann::json cellsToJsonArray(const std::vector<CellInfo>& cells);
    static nlohmann::json coordsToJsonArray(const std::vector<std::pair<int, int>>& coords);
};

}  // namespace seabattle