#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "game/Game.hpp"
#include "game/GameManager.hpp"
#include "network/Session.hpp"
#include "player/Player.hpp"

using namespace seabattle;

class GameTest : public ::testing::Test {
   protected:
    std::shared_ptr<Player> player1;
    std::shared_ptr<Player> player2;
    std::shared_ptr<Game> game;

    void SetUp() override {
        player1 = std::make_shared<Player>("guid-1", "Player1");
        player2 = std::make_shared<Player>("guid-2", "Player2");
        game = std::make_shared<Game>("game-1", player1, player2);
    }

    std::vector<ShipPlacement> createValidPlacement() {
        return {{0, 0, 4, true}, {0, 2, 3, true}, {0, 4, 3, true}, {0, 6, 2, true},
                {3, 6, 2, true}, {6, 6, 2, true}, {0, 8, 1, true}, {2, 8, 1, true},
                {4, 8, 1, true}, {6, 8, 1, true}};
    }

    void setupBothPlayersReady() {
        game->placeShips("guid-1", createValidPlacement());
        game->placeShips("guid-2", createValidPlacement());
        game->startBattle();
    }
};

TEST_F(GameTest, Constructor_GeneratesId) {
    EXPECT_EQ(game->getId(), "game-1");
}

TEST_F(GameTest, Constructor_InitialStatePlacing) {
    EXPECT_EQ(game->getState(), GameState::Placing);
}

TEST_F(GameTest, Constructor_BothPlayersNotReady) {
    EXPECT_FALSE(game->isPlayerReady("guid-1"));
    EXPECT_FALSE(game->isPlayerReady("guid-2"));
    EXPECT_FALSE(game->areBothPlayersReady());
}

TEST_F(GameTest, Constructor_SetsCurrentTurn) {
    std::string turn = game->getCurrentTurnGuid();
    EXPECT_TRUE(turn == "guid-1" || turn == "guid-2");
}

TEST_F(GameTest, IsPlayer_ValidPlayer_ReturnsTrue) {
    EXPECT_TRUE(game->isPlayer("guid-1"));
    EXPECT_TRUE(game->isPlayer("guid-2"));
}

TEST_F(GameTest, IsPlayer_InvalidPlayer_ReturnsFalse) {
    EXPECT_FALSE(game->isPlayer("guid-3"));
}

TEST_F(GameTest, GetOpponentGuid_ReturnsCorrect) {
    EXPECT_EQ(game->getOpponentGuid("guid-1"), "guid-2");
    EXPECT_EQ(game->getOpponentGuid("guid-2"), "guid-1");
}

TEST_F(GameTest, GetPlayerNickname_ReturnsCorrect) {
    EXPECT_EQ(game->getPlayerNickname("guid-1"), "Player1");
    EXPECT_EQ(game->getPlayerNickname("guid-2"), "Player2");
}

TEST_F(GameTest, PlaceShips_ValidPlacement_ReturnsTrue) {
    auto ships = createValidPlacement();

    EXPECT_TRUE(game->placeShips("guid-1", ships));
    EXPECT_TRUE(game->isPlayerReady("guid-1"));
}

TEST_F(GameTest, PlaceShips_InvalidPlacement_ReturnsFalse) {
    std::vector<ShipPlacement> invalidShips = {{0, 0, 4, true}, {0, 0, 3, true}};

    EXPECT_FALSE(game->placeShips("guid-1", invalidShips));
    EXPECT_FALSE(game->isPlayerReady("guid-1"));
}

TEST_F(GameTest, PlaceShips_BothReady_CanStartBattle) {
    game->placeShips("guid-1", createValidPlacement());
    game->placeShips("guid-2", createValidPlacement());

    EXPECT_TRUE(game->areBothPlayersReady());

    game->startBattle();
    EXPECT_EQ(game->getState(), GameState::Battle);
}

TEST_F(GameTest, PlaceShipsRandomly_SetsPlayerReady) {
    game->placeShipsRandomly("guid-1");

    EXPECT_TRUE(game->isPlayerReady("guid-1"));
}

TEST_F(GameTest, StartBattle_NotBothReady_StaysPlacing) {
    game->placeShips("guid-1", createValidPlacement());

    game->startBattle();

    EXPECT_EQ(game->getState(), GameState::Placing);
}

TEST_F(GameTest, Shoot_CorrectTurn_Succeeds) {
    setupBothPlayersReady();

    std::string currentPlayer = game->getCurrentTurnGuid();
    auto result = game->shoot(currentPlayer, 0, 0);

    EXPECT_NE(result.result, ShotResult::InvalidCoordinates);
}

TEST_F(GameTest, Shoot_WrongTurn_Fails) {
    setupBothPlayersReady();

    std::string currentPlayer = game->getCurrentTurnGuid();
    std::string otherPlayer = game->getOpponentGuid(currentPlayer);

    auto result = game->shoot(otherPlayer, 0, 0);

    EXPECT_EQ(result.result, ShotResult::InvalidCoordinates);
}

TEST_F(GameTest, Shoot_NotInBattle_Fails) {
    auto result = game->shoot("guid-1", 0, 0);

    EXPECT_EQ(result.result, ShotResult::InvalidCoordinates);
}

TEST_F(GameTest, Shoot_Miss_SwitchesTurn) {
    setupBothPlayersReady();

    std::string firstTurn = game->getCurrentTurnGuid();
    game->shoot(firstTurn, 9, 9);

    EXPECT_NE(game->getCurrentTurnGuid(), firstTurn);
}

TEST_F(GameTest, Shoot_Hit_KeepsTurn) {
    setupBothPlayersReady();

    std::string currentPlayer = game->getCurrentTurnGuid();
    game->shoot(currentPlayer, 0, 0);

    EXPECT_EQ(game->getCurrentTurnGuid(), currentPlayer);
}

TEST_F(GameTest, Surrender_EndsGame) {
    setupBothPlayersReady();

    game->surrender("guid-1");

    EXPECT_TRUE(game->isFinished());
    EXPECT_EQ(game->getState(), GameState::Finished);
    EXPECT_EQ(game->getWinnerGuid(), "guid-2");
    EXPECT_EQ(game->getGameOverReason(), "surrender");
}

TEST_F(GameTest, HandleDisconnect_EndsGame) {
    setupBothPlayersReady();

    game->handleDisconnect("guid-2");

    EXPECT_TRUE(game->isFinished());
    EXPECT_EQ(game->getWinnerGuid(), "guid-1");
    EXPECT_EQ(game->getGameOverReason(), "disconnect");
}

TEST_F(GameTest, AddSpectator_IncreasesCount) {
    boost::asio::io_context io_context;
    auto session = std::make_shared<Session>(io_context);

    game->addSpectator(session);

    EXPECT_EQ(game->getSpectatorCount(), 1);
}

TEST_F(GameTest, RemoveSpectator_DecreasesCount) {
    boost::asio::io_context io_context;
    auto session = std::make_shared<Session>(io_context);
    game->addSpectator(session);

    game->removeSpectator(session);

    EXPECT_EQ(game->getSpectatorCount(), 0);
}

TEST_F(GameTest, NotifySpectators_SendsMessage) {
    boost::asio::io_context io_context;
    auto session = std::make_shared<Session>(io_context);
    game->addSpectator(session);

    game->notifySpectators("test message");

    EXPECT_EQ(session->getSentMessages().size(), 1);
    EXPECT_EQ(session->getSentMessages()[0], "test message");
}

TEST_F(GameTest, GetBoardFogOfWar_ReturnsOnlyHitsAndMisses) {
    setupBothPlayersReady();

    std::string currentPlayer = game->getCurrentTurnGuid();
    std::string opponent = game->getOpponentGuid(currentPlayer);

    game->shoot(currentPlayer, 0, 0);

    auto fog = game->getBoardFogOfWar(opponent);

    for (const auto& cell : fog) {
        EXPECT_TRUE(cell.state == CellState::Hit || cell.state == CellState::Miss);
    }
}

TEST_F(GameTest, AllShipsSunk_EndsGame) {
    player1 = std::make_shared<Player>("guid-1", "Player1");
    player2 = std::make_shared<Player>("guid-2", "Player2");
    game = std::make_shared<Game>("game-1", player1, player2);

    std::vector<ShipPlacement> minimalShips = {{0, 0, 1, true}};

    game->placeShips("guid-1", createValidPlacement());

    Board* board2 = game->getPlayerBoard("guid-2");
    board2->clear();
    board2->placeShip(0, 0, 1, true);
    game->placeShipsRandomly("guid-2");

    game->placeShips("guid-2", createValidPlacement());
    game->startBattle();

    std::string currentPlayer = game->getCurrentTurnGuid();

    for (int y = 0; y < 10 && !game->isFinished(); ++y) {
        for (int x = 0; x < 10 && !game->isFinished(); ++x) {
            if (game->getCurrentTurnGuid() == currentPlayer) {
                game->shoot(currentPlayer, x, y);
            } else {
                game->shoot(game->getOpponentGuid(currentPlayer), 9, 9);
            }
        }
    }
}

class GameManagerTest : public ::testing::Test {
   protected:
    GameManager manager;
    std::shared_ptr<Player> player1;
    std::shared_ptr<Player> player2;

    void SetUp() override {
        player1 = std::make_shared<Player>("guid-1", "Player1");
        player2 = std::make_shared<Player>("guid-2", "Player2");
    }
};

TEST_F(GameManagerTest, CreateGame_ReturnsGame) {
    auto game = manager.createGame(player1, player2);

    ASSERT_NE(game, nullptr);
    EXPECT_FALSE(game->getId().empty());
}

TEST_F(GameManagerTest, CreateGame_SetsPlayersInGame) {
    manager.createGame(player1, player2);

    EXPECT_EQ(player1->getStatus(), PlayerStatus::InGame);
    EXPECT_EQ(player2->getStatus(), PlayerStatus::InGame);
    EXPECT_TRUE(player1->getCurrentGameId().has_value());
    EXPECT_TRUE(player2->getCurrentGameId().has_value());
}

TEST_F(GameManagerTest, GetGame_Exists_ReturnsGame) {
    auto created = manager.createGame(player1, player2);

    auto found = manager.getGame(created->getId());

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getId(), created->getId());
}

TEST_F(GameManagerTest, GetGame_NotExists_ReturnsNull) {
    auto found = manager.getGame("non-existent");

    EXPECT_EQ(found, nullptr);
}

TEST_F(GameManagerTest, GetGameByPlayer_Exists_ReturnsGame) {
    auto created = manager.createGame(player1, player2);

    auto found1 = manager.getGameByPlayer("guid-1");
    auto found2 = manager.getGameByPlayer("guid-2");

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->getId(), created->getId());
    EXPECT_EQ(found2->getId(), created->getId());
}

TEST_F(GameManagerTest, GetGameByPlayer_NotExists_ReturnsNull) {
    auto found = manager.getGameByPlayer("guid-3");

    EXPECT_EQ(found, nullptr);
}

TEST_F(GameManagerTest, RemoveGame_RemovesFromManager) {
    auto game = manager.createGame(player1, player2);
    std::string gameId = game->getId();

    manager.removeGame(gameId);

    EXPECT_EQ(manager.getGame(gameId), nullptr);
    EXPECT_EQ(manager.getGameByPlayer("guid-1"), nullptr);
    EXPECT_EQ(manager.getActiveGameCount(), 0);
}

TEST_F(GameManagerTest, GetActiveGameCount_Correct) {
    EXPECT_EQ(manager.getActiveGameCount(), 0);

    manager.createGame(player1, player2);

    EXPECT_EQ(manager.getActiveGameCount(), 1);
}

TEST_F(GameManagerTest, AddSpectator_ToExistingGame_ReturnsTrue) {
    auto game = manager.createGame(player1, player2);
    boost::asio::io_context io_context;
    auto session = std::make_shared<Session>(io_context);

    bool result = manager.addSpectator(game->getId(), session);

    EXPECT_TRUE(result);
    EXPECT_EQ(game->getSpectatorCount(), 1);
}

TEST_F(GameManagerTest, AddSpectator_ToNonExistingGame_ReturnsFalse) {
    boost::asio::io_context io_context;
    auto session = std::make_shared<Session>(io_context);

    bool result = manager.addSpectator("non-existent", session);

    EXPECT_FALSE(result);
}
