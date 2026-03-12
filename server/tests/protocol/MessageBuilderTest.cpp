#include <gtest/gtest.h>

#include "player/PlayerStats.hpp"
#include "protocol/MessageBuilder.hpp"

using namespace seabattle;

class MessageBuilderTest : public ::testing::Test {
   protected:
    nlohmann::json parseJson(const std::string& str) {
        return nlohmann::json::parse(str);
    }
};

TEST_F(MessageBuilderTest, BuildAuthResponse_Success_CorrectFormat) {
    auto result = MessageBuilder::buildAuthResponse(true, "guid-123", "Player1");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "auth");
    EXPECT_TRUE(json["payload"]["success"].get<bool>());
    EXPECT_EQ(json["payload"]["guid"], "guid-123");
    EXPECT_EQ(json["payload"]["nickname"], "Player1");
}

TEST_F(MessageBuilderTest, BuildAuthError_CorrectFormat) {
    auto result = MessageBuilder::buildAuthError(1001, "Invalid nickname");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "auth");
    EXPECT_FALSE(json["payload"]["success"].get<bool>());
    EXPECT_EQ(json["payload"]["error_code"], 1001);
    EXPECT_EQ(json["payload"]["error"], "Invalid nickname");
}

TEST_F(MessageBuilderTest, BuildPlayerList_CorrectFormat) {
    std::vector<PlayerInfo> players = {{"guid-1", "Player1", PlayerStatus::Online, std::nullopt},
                                       {"guid-2", "Player2", PlayerStatus::InGame, "game-123"}};

    auto result = MessageBuilder::buildPlayerList(players);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "get_players");
    EXPECT_EQ(json["payload"]["players"].size(), 2);
    EXPECT_EQ(json["payload"]["players"][0]["guid"], "guid-1");
    EXPECT_EQ(json["payload"]["players"][0]["status"], "online");
    EXPECT_EQ(json["payload"]["players"][1]["status"], "in_game");
    EXPECT_EQ(json["payload"]["players"][1]["game_id"], "game-123");
}

TEST_F(MessageBuilderTest, BuildFriendList_CorrectFormat) {
    std::vector<FriendInfo> friends = {{"guid-1", "Friend1", PlayerStatus::Online},
                                       {"guid-2", "Friend2", PlayerStatus::Offline}};

    auto result = MessageBuilder::buildFriendList(friends);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "get_friends");
    EXPECT_EQ(json["payload"]["friends"].size(), 2);
    EXPECT_EQ(json["payload"]["friends"][0]["nickname"], "Friend1");
    EXPECT_EQ(json["payload"]["friends"][1]["status"], "offline");
}

TEST_F(MessageBuilderTest, BuildStatsResponse_CorrectFormat) {
    PlayerStats stats;
    stats.recordWin();
    stats.recordWin();
    stats.recordLoss();
    stats.recordShot(true);
    stats.recordShot(false);

    auto result = MessageBuilder::buildStatsResponse("guid-1", "Player1", stats);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "get_stats");
    EXPECT_EQ(json["payload"]["guid"], "guid-1");
    EXPECT_EQ(json["payload"]["nickname"], "Player1");
    EXPECT_EQ(json["payload"]["games_played"], 3);
    EXPECT_EQ(json["payload"]["wins"], 2);
    EXPECT_EQ(json["payload"]["losses"], 1);
    EXPECT_EQ(json["payload"]["shots_fired"], 2);
    EXPECT_EQ(json["payload"]["shots_hit"], 1);
}

TEST_F(MessageBuilderTest, BuildPlayerStatusChanged_Online_CorrectFormat) {
    auto result = MessageBuilder::buildPlayerStatusChanged("guid-1", PlayerStatus::Online);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "player_status_changed");
    EXPECT_EQ(json["payload"]["guid"], "guid-1");
    EXPECT_EQ(json["payload"]["status"], "online");
    EXPECT_FALSE(json["payload"].contains("game_id"));
}

TEST_F(MessageBuilderTest, BuildPlayerStatusChanged_InGame_IncludesGameId) {
    auto result =
        MessageBuilder::buildPlayerStatusChanged("guid-1", PlayerStatus::InGame, "game-123");
    auto json = parseJson(result);

    EXPECT_EQ(json["payload"]["status"], "in_game");
    EXPECT_EQ(json["payload"]["game_id"], "game-123");
}

TEST_F(MessageBuilderTest, BuildInviteReceived_CorrectFormat) {
    auto result = MessageBuilder::buildInviteReceived("invite-1", "from-guid", "FromPlayer");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "invite_received");
    EXPECT_EQ(json["payload"]["invite_id"], "invite-1");
    EXPECT_EQ(json["payload"]["from_guid"], "from-guid");
    EXPECT_EQ(json["payload"]["from_nickname"], "FromPlayer");
}

TEST_F(MessageBuilderTest, BuildGameStarted_CorrectFormat) {
    auto result = MessageBuilder::buildGameStarted("game-1", "opp-guid", "Opponent");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "game_started");
    EXPECT_EQ(json["payload"]["game_id"], "game-1");
    EXPECT_EQ(json["payload"]["opponent"]["guid"], "opp-guid");
    EXPECT_EQ(json["payload"]["opponent"]["nickname"], "Opponent");
}

TEST_F(MessageBuilderTest, BuildShotResult_Hit_CorrectFormat) {
    ShotResultInfo info;
    info.result = ShotResult::Hit;
    info.x = 5;
    info.y = 3;
    info.next_turn_guid = "guid-1";

    auto result = MessageBuilder::buildShotResult(info);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "shoot");
    EXPECT_EQ(json["payload"]["result"], "hit");
    EXPECT_EQ(json["payload"]["x"], 5);
    EXPECT_EQ(json["payload"]["y"], 3);
    EXPECT_EQ(json["payload"]["next_turn"], "guid-1");
}

TEST_F(MessageBuilderTest, BuildShotResult_Kill_IncludesShipInfo) {
    ShotResultInfo info;
    info.result = ShotResult::Kill;
    info.x = 5;
    info.y = 3;
    info.next_turn_guid = "guid-1";
    info.killed_ship_cells = std::vector<std::pair<int, int>>{{5, 3}, {5, 4}};
    info.surrounding_misses = std::vector<std::pair<int, int>>{{4, 2}, {4, 3}};

    auto result = MessageBuilder::buildShotResult(info);
    auto json = parseJson(result);

    EXPECT_EQ(json["payload"]["result"], "kill");
    EXPECT_TRUE(json["payload"].contains("ship_killed"));
    EXPECT_EQ(json["payload"]["ship_killed"]["cells"].size(), 2);
    EXPECT_EQ(json["payload"]["ship_killed"]["surrounding_misses"].size(), 2);
}

TEST_F(MessageBuilderTest, BuildOpponentShot_CorrectFormat) {
    auto result = MessageBuilder::buildOpponentShot(2, 7, ShotResult::Miss, true);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "opponent_shot");
    EXPECT_EQ(json["payload"]["x"], 2);
    EXPECT_EQ(json["payload"]["y"], 7);
    EXPECT_EQ(json["payload"]["result"], "miss");
    EXPECT_TRUE(json["payload"]["your_turn"].get<bool>());
}

TEST_F(MessageBuilderTest, BuildGameOver_CorrectFormat) {
    std::vector<CellInfo> board1 = {{0, 0, CellState::Ship}, {1, 0, CellState::Hit}};
    std::vector<CellInfo> board2 = {{5, 5, CellState::Miss}};

    auto result = MessageBuilder::buildGameOver("winner-guid", "all_ships_sunk", board1, board2);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "game_over");
    EXPECT_EQ(json["payload"]["winner_guid"], "winner-guid");
    EXPECT_EQ(json["payload"]["reason"], "all_ships_sunk");
    EXPECT_EQ(json["payload"]["board1"].size(), 2);
    EXPECT_EQ(json["payload"]["board2"].size(), 1);
}

TEST_F(MessageBuilderTest, BuildSpectateResponse_Success_CorrectFormat) {
    std::vector<CellInfo> fog1 = {{0, 0, CellState::Hit}};
    std::vector<CellInfo> fog2 = {{5, 5, CellState::Miss}};

    auto result = MessageBuilder::buildSpectateResponse(true, "p1-guid", "Player1", "p2-guid",
                                                        "Player2", "p1-guid", fog1, fog2);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "spectate");
    EXPECT_TRUE(json["payload"]["success"].get<bool>());
    EXPECT_EQ(json["payload"]["game_state"]["player1"]["guid"], "p1-guid");
    EXPECT_EQ(json["payload"]["game_state"]["player2"]["nickname"], "Player2");
    EXPECT_EQ(json["payload"]["game_state"]["current_turn"], "p1-guid");
}

TEST_F(MessageBuilderTest, BuildSpectateUpdate_CorrectFormat) {
    auto result =
        MessageBuilder::buildSpectateUpdate("shooter-guid", 3, 4, ShotResult::Hit, "next-guid");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "spectate_update");
    EXPECT_EQ(json["payload"]["shooter_guid"], "shooter-guid");
    EXPECT_EQ(json["payload"]["x"], 3);
    EXPECT_EQ(json["payload"]["y"], 4);
    EXPECT_EQ(json["payload"]["result"], "hit");
    EXPECT_EQ(json["payload"]["next_turn"], "next-guid");
}

TEST_F(MessageBuilderTest, BuildError_CorrectFormat) {
    auto result = MessageBuilder::buildError(2001, "Game not found");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "error");
    EXPECT_EQ(json["payload"]["error_code"], 2001);
    EXPECT_EQ(json["payload"]["error"], "Game not found");
}

TEST_F(MessageBuilderTest, BuildSuccess_CorrectFormat) {
    auto result = MessageBuilder::buildSuccess("add_friend");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "add_friend");
    EXPECT_TRUE(json["payload"]["success"].get<bool>());
}

TEST_F(MessageBuilderTest, BuildFindGameResponse_CorrectFormat) {
    auto result = MessageBuilder::buildFindGameResponse(true, 5);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "find_game");
    EXPECT_TRUE(json["payload"]["in_queue"].get<bool>());
    EXPECT_EQ(json["payload"]["queue_size"], 5);
}

TEST_F(MessageBuilderTest, BuildPlaceShipsResponse_Success_CorrectFormat) {
    auto result = MessageBuilder::buildPlaceShipsResponse(true);
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "place_ships");
    EXPECT_TRUE(json["payload"]["success"].get<bool>());
    EXPECT_FALSE(json["payload"].contains("error"));
}

TEST_F(MessageBuilderTest, BuildPlaceShipsResponse_Failure_IncludesError) {
    auto result = MessageBuilder::buildPlaceShipsResponse(false, "Ships overlap");
    auto json = parseJson(result);

    EXPECT_FALSE(json["payload"]["success"].get<bool>());
    EXPECT_EQ(json["payload"]["error"], "Ships overlap");
}

TEST_F(MessageBuilderTest, BuildPlayerReady_CorrectFormat) {
    auto result = MessageBuilder::buildPlayerReady("guid-1");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "player_ready");
    EXPECT_EQ(json["payload"]["player_guid"], "guid-1");
}

TEST_F(MessageBuilderTest, BuildBattleStart_CorrectFormat) {
    auto result = MessageBuilder::buildBattleStart("first-guid");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "event");
    EXPECT_EQ(json["action"], "battle_start");
    EXPECT_EQ(json["payload"]["first_turn"], "first-guid");
}

TEST_F(MessageBuilderTest, BuildInviteResponse_CorrectFormat) {
    auto result = MessageBuilder::buildInviteResponse(true, "invite-123");
    auto json = parseJson(result);

    EXPECT_EQ(json["type"], "response");
    EXPECT_EQ(json["action"], "invite_response");
    EXPECT_TRUE(json["payload"]["success"].get<bool>());
    EXPECT_EQ(json["payload"]["invite_id"], "invite-123");
}
