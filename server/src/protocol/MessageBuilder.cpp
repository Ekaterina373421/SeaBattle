#include "MessageBuilder.hpp"

#include "player/PlayerStats.hpp"

namespace seabattle {

nlohmann::json MessageBuilder::createBase(const std::string& type, const std::string& action) {
    nlohmann::json msg;
    msg["type"] = type;
    msg["action"] = action;
    msg["payload"] = nlohmann::json::object();
    return msg;
}

std::string MessageBuilder::shotResultToString(ShotResult result) {
    switch (result) {
        case ShotResult::Miss:
            return "miss";
        case ShotResult::Hit:
            return "hit";
        case ShotResult::Kill:
            return "kill";
        case ShotResult::AlreadyShot:
            return "already_shot";
        case ShotResult::InvalidCoordinates:
            return "invalid";
    }
    return "unknown";
}

std::string MessageBuilder::playerStatusToString(PlayerStatus status) {
    switch (status) {
        case PlayerStatus::Offline:
            return "offline";
        case PlayerStatus::Online:
            return "online";
        case PlayerStatus::InGame:
            return "in_game";
    }
    return "unknown";
}

nlohmann::json MessageBuilder::cellInfoToJson(const CellInfo& cell) {
    std::string stateStr;
    switch (cell.state) {
        case CellState::Empty:
            stateStr = "empty";
            break;
        case CellState::Ship:
            stateStr = "ship";
            break;
        case CellState::Hit:
            stateStr = "hit";
            break;
        case CellState::Miss:
            stateStr = "miss";
            break;
    }
    return nlohmann::json::array({cell.x, cell.y, stateStr});
}

nlohmann::json MessageBuilder::cellsToJsonArray(const std::vector<CellInfo>& cells) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& cell : cells) {
        arr.push_back(cellInfoToJson(cell));
    }
    return arr;
}

nlohmann::json MessageBuilder::coordsToJsonArray(const std::vector<std::pair<int, int>>& coords) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [x, y] : coords) {
        arr.push_back(nlohmann::json::array({x, y}));
    }
    return arr;
}

std::string MessageBuilder::buildAuthResponse(bool success, const std::string& guid,
                                              const std::string& nickname) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::AUTH);
    msg["payload"]["success"] = success;
    msg["payload"]["guid"] = guid;
    msg["payload"]["nickname"] = nickname;
    return msg.dump();
}

std::string MessageBuilder::buildAuthError(int errorCode, const std::string& message) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::AUTH);
    msg["payload"]["success"] = false;
    msg["payload"]["error_code"] = errorCode;
    msg["payload"]["error"] = message;
    return msg.dump();
}

std::string MessageBuilder::buildPlayerList(const std::vector<PlayerInfo>& players) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::GET_PLAYERS);

    nlohmann::json playersArr = nlohmann::json::array();
    for (const auto& p : players) {
        nlohmann::json pJson;
        pJson["guid"] = p.guid;
        pJson["nickname"] = p.nickname;
        pJson["status"] = playerStatusToString(p.status);
        if (p.game_id.has_value()) {
            pJson["game_id"] = *p.game_id;
        }
        playersArr.push_back(pJson);
    }

    msg["payload"]["players"] = playersArr;
    return msg.dump();
}

std::string MessageBuilder::buildFriendList(const std::vector<FriendInfo>& friends) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::GET_FRIENDS);

    nlohmann::json friendsArr = nlohmann::json::array();
    for (const auto& f : friends) {
        nlohmann::json fJson;
        fJson["guid"] = f.guid;
        fJson["nickname"] = f.nickname;
        fJson["status"] = playerStatusToString(f.status);
        friendsArr.push_back(fJson);
    }

    msg["payload"]["friends"] = friendsArr;
    return msg.dump();
}

std::string MessageBuilder::buildStatsResponse(const std::string& guid, const std::string& nickname,
                                               const PlayerStats& stats) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::GET_STATS);
    msg["payload"]["guid"] = guid;
    msg["payload"]["nickname"] = nickname;
    msg["payload"]["games_played"] = stats.getGamesPlayed();
    msg["payload"]["wins"] = stats.getWins();
    msg["payload"]["losses"] = stats.getLosses();
    msg["payload"]["winrate"] = stats.getWinrate();
    msg["payload"]["shots_fired"] = stats.getShotsFired();
    msg["payload"]["shots_hit"] = stats.getShotsHit();
    msg["payload"]["accuracy"] = stats.getAccuracy();
    return msg.dump();
}

std::string MessageBuilder::buildPlayerStatusChanged(const std::string& guid, PlayerStatus status,
                                                     const std::optional<std::string>& gameId) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::PLAYER_STATUS_CHANGED);
    msg["payload"]["guid"] = guid;
    msg["payload"]["status"] = playerStatusToString(status);
    if (gameId.has_value()) {
        msg["payload"]["game_id"] = *gameId;
    }
    return msg.dump();
}

std::string MessageBuilder::buildInviteReceived(const std::string& inviteId,
                                                const std::string& fromGuid,
                                                const std::string& fromNickname) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::INVITE_RECEIVED);
    msg["payload"]["invite_id"] = inviteId;
    msg["payload"]["from_guid"] = fromGuid;
    msg["payload"]["from_nickname"] = fromNickname;
    return msg.dump();
}

std::string MessageBuilder::buildInviteResponse(bool success, const std::string& inviteId) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::INVITE_RESPONSE);
    msg["payload"]["success"] = success;
    msg["payload"]["invite_id"] = inviteId;
    return msg.dump();
}

std::string MessageBuilder::buildGameStarted(const std::string& gameId,
                                             const std::string& opponentGuid,
                                             const std::string& opponentNickname) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::GAME_STARTED);
    msg["payload"]["game_id"] = gameId;
    msg["payload"]["opponent"]["guid"] = opponentGuid;
    msg["payload"]["opponent"]["nickname"] = opponentNickname;
    return msg.dump();
}

std::string MessageBuilder::buildFindGameResponse(bool inQueue, size_t queueSize) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::FIND_GAME);
    msg["payload"]["in_queue"] = inQueue;
    msg["payload"]["queue_size"] = queueSize;
    return msg.dump();
}

std::string MessageBuilder::buildPlaceShipsResponse(bool success, const std::string& error) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::PLACE_SHIPS);
    msg["payload"]["success"] = success;
    if (!success && !error.empty()) {
        msg["payload"]["error"] = error;
    }
    return msg.dump();
}

std::string MessageBuilder::buildPlayerReady(const std::string& playerGuid) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::PLAYER_READY);
    msg["payload"]["player_guid"] = playerGuid;
    return msg.dump();
}

std::string MessageBuilder::buildBattleStart(const std::string& firstTurnGuid) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::BATTLE_START);
    msg["payload"]["first_turn"] = firstTurnGuid;
    return msg.dump();
}

std::string MessageBuilder::buildShotResult(const ShotResultInfo& info) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::SHOOT);
    msg["payload"]["result"] = shotResultToString(info.result);
    msg["payload"]["x"] = info.x;
    msg["payload"]["y"] = info.y;
    msg["payload"]["next_turn"] = info.next_turn_guid;

    if (info.killed_ship_cells.has_value()) {
        msg["payload"]["ship_killed"]["cells"] = coordsToJsonArray(*info.killed_ship_cells);
    }

    if (info.surrounding_misses.has_value()) {
        msg["payload"]["ship_killed"]["surrounding_misses"] =
            coordsToJsonArray(*info.surrounding_misses);
    }

    return msg.dump();
}

std::string MessageBuilder::buildOpponentShot(
    int x, int y, ShotResult result, bool yourTurn,
    const std::optional<std::vector<std::pair<int, int>>>& killedCells,
    const std::optional<std::vector<std::pair<int, int>>>& surroundingMisses) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::OPPONENT_SHOT);
    msg["payload"]["x"] = x;
    msg["payload"]["y"] = y;
    msg["payload"]["result"] = shotResultToString(result);
    msg["payload"]["your_turn"] = yourTurn;

    if (killedCells.has_value()) {
        msg["payload"]["ship_killed"]["cells"] = coordsToJsonArray(*killedCells);
    }

    if (surroundingMisses.has_value()) {
        msg["payload"]["ship_killed"]["surrounding_misses"] = coordsToJsonArray(*surroundingMisses);
    }

    return msg.dump();
}

std::string MessageBuilder::buildGameOver(const std::string& winnerGuid, const std::string& reason,
                                          const std::vector<CellInfo>& board1,
                                          const std::vector<CellInfo>& board2) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::GAME_OVER);
    msg["payload"]["winner_guid"] = winnerGuid;
    msg["payload"]["reason"] = reason;
    msg["payload"]["board1"] = cellsToJsonArray(board1);
    msg["payload"]["board2"] = cellsToJsonArray(board2);
    return msg.dump();
}

std::string MessageBuilder::buildSpectateResponse(bool success, const std::string& player1Guid,
                                                  const std::string& player1Nickname,
                                                  const std::string& player2Guid,
                                                  const std::string& player2Nickname,
                                                  const std::string& currentTurn,
                                                  const std::vector<CellInfo>& board1Fog,
                                                  const std::vector<CellInfo>& board2Fog) {
    auto msg = createBase(protocol::message_type::RESPONSE, protocol::action::SPECTATE);
    msg["payload"]["success"] = success;

    if (success) {
        msg["payload"]["game_state"]["player1"]["guid"] = player1Guid;
        msg["payload"]["game_state"]["player1"]["nickname"] = player1Nickname;
        msg["payload"]["game_state"]["player2"]["guid"] = player2Guid;
        msg["payload"]["game_state"]["player2"]["nickname"] = player2Nickname;
        msg["payload"]["game_state"]["current_turn"] = currentTurn;
        msg["payload"]["game_state"]["board1_fog"] = cellsToJsonArray(board1Fog);
        msg["payload"]["game_state"]["board2_fog"] = cellsToJsonArray(board2Fog);
    }

    return msg.dump();
}

std::string MessageBuilder::buildSpectateUpdate(const std::string& shooterGuid, int x, int y,
                                                ShotResult result, const std::string& nextTurn) {
    auto msg = createBase(protocol::message_type::EVENT, protocol::action::SPECTATE_UPDATE);
    msg["payload"]["shooter_guid"] = shooterGuid;
    msg["payload"]["x"] = x;
    msg["payload"]["y"] = y;
    msg["payload"]["result"] = shotResultToString(result);
    msg["payload"]["next_turn"] = nextTurn;
    return msg.dump();
}

std::string MessageBuilder::buildError(int errorCode, const std::string& message) {
    nlohmann::json msg;
    msg["type"] = protocol::message_type::RESPONSE;
    msg["action"] = "error";
    msg["payload"]["error_code"] = errorCode;
    msg["payload"]["error"] = message;
    return msg.dump();
}

std::string MessageBuilder::buildSuccess(const std::string& action) {
    auto msg = createBase(protocol::message_type::RESPONSE, action);
    msg["payload"]["success"] = true;
    return msg.dump();
}

}  // namespace seabattle
