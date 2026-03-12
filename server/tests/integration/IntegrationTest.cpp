#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

#include "game/Game.hpp"
#include "game/GameManager.hpp"
#include "lobby/Lobby.hpp"
#include "network/MessageHandler.hpp"
#include "network/Server.hpp"
#include "network/Session.hpp"
#include "network/SessionManager.hpp"
#include "player/Player.hpp"
#include "player/PlayerManager.hpp"
#include "protocol/MessageBuilder.hpp"
#include "protocol/MessageParser.hpp"

using namespace seabattle;
using namespace std::chrono_literals;

class MockNetworkSession : public std::enable_shared_from_this<MockNetworkSession> {
   public:
    explicit MockNetworkSession(uint64_t id) : id_(id) {}

    void send(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        received_messages_.push_back(message);
    }

    std::vector<std::string> getMessages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_messages_;
    }

    void clearMessages() {
        std::lock_guard<std::mutex> lock(mutex_);
        received_messages_.clear();
    }

    size_t getMessageCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_messages_.size();
    }

    std::string getLastMessage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (received_messages_.empty())
            return "";
        return received_messages_.back();
    }

    nlohmann::json getLastMessageJson() const {
        auto msg = getLastMessage();
        if (msg.empty())
            return {};
        return nlohmann::json::parse(msg);
    }

    uint64_t getId() const {
        return id_;
    }

   private:
    uint64_t id_;
    mutable std::mutex mutex_;
    std::vector<std::string> received_messages_;
};

class IntegrationTestFixture : public ::testing::Test {
   protected:
    std::shared_ptr<PlayerManager> playerManager;
    std::shared_ptr<GameManager> gameManager;
    std::shared_ptr<Lobby> lobby;

    void SetUp() override {
        playerManager = std::make_shared<PlayerManager>();
        gameManager = std::make_shared<GameManager>();
        lobby = std::make_shared<Lobby>(gameManager);
    }

    std::shared_ptr<Player> createAndAuthPlayer(const std::string& nickname) {
        auto player = playerManager->createPlayer(nickname);
        player->setStatus(PlayerStatus::Online);
        return player;
    }

    std::vector<ShipPlacement> createValidPlacement() {
        return {{0, 0, 4, true}, {0, 2, 3, true}, {0, 4, 3, true}, {0, 6, 2, true},
                {3, 6, 2, true}, {6, 6, 2, true}, {0, 8, 1, true}, {2, 8, 1, true},
                {4, 8, 1, true}, {6, 8, 1, true}};
    }

    std::vector<ShipPlacement> createAlternativePlacement() {
        return {{5, 0, 4, false}, {7, 0, 3, false}, {9, 0, 3, false}, {0, 0, 2, true},
                {0, 2, 2, true},  {0, 4, 2, true},  {3, 0, 1, true},  {3, 2, 1, true},
                {3, 4, 1, true},  {3, 6, 1, true}};
    }
};

TEST_F(IntegrationTestFixture, FullGameCycle_TwoPlayers) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    ASSERT_NE(player1, nullptr);
    ASSERT_NE(player2, nullptr);
    EXPECT_NE(player1->getGuid(), player2->getGuid());

    auto game = gameManager->createGame(player1, player2);

    ASSERT_NE(game, nullptr);
    EXPECT_EQ(game->getState(), GameState::Placing);
    EXPECT_EQ(player1->getStatus(), PlayerStatus::InGame);
    EXPECT_EQ(player2->getStatus(), PlayerStatus::InGame);

    auto ships1 = createValidPlacement();
    auto ships2 = createAlternativePlacement();

    EXPECT_TRUE(game->placeShips(player1->getGuid(), ships1));
    EXPECT_TRUE(game->isPlayerReady(player1->getGuid()));
    EXPECT_FALSE(game->areBothPlayersReady());

    EXPECT_TRUE(game->placeShips(player2->getGuid(), ships2));
    EXPECT_TRUE(game->isPlayerReady(player2->getGuid()));
    EXPECT_TRUE(game->areBothPlayersReady());

    game->startBattle();
    EXPECT_EQ(game->getState(), GameState::Battle);

    std::string currentTurn = game->getCurrentTurnGuid();
    EXPECT_TRUE(currentTurn == player1->getGuid() || currentTurn == player2->getGuid());

    int maxShots = 200;
    int shotCount = 0;

    while (!game->isFinished() && shotCount < maxShots) {
        std::string shooter = game->getCurrentTurnGuid();

        bool shotMade = false;
        for (int y = 0; y < 10 && !shotMade && !game->isFinished(); ++y) {
            for (int x = 0; x < 10 && !shotMade && !game->isFinished(); ++x) {
                if (game->getCurrentTurnGuid() != shooter)
                    break;

                auto result = game->shoot(shooter, x, y);

                if (result.result == ShotResult::AlreadyShot) {
                    continue;
                }

                ++shotCount;

                if (result.result == ShotResult::Miss) {
                    shotMade = true;
                }
            }
        }
    }
    EXPECT_TRUE(game->isFinished());
    EXPECT_EQ(game->getState(), GameState::Finished);
    EXPECT_TRUE(game->getWinnerGuid().has_value());
    EXPECT_EQ(game->getGameOverReason(), "all_ships_sunk");
}

TEST_F(IntegrationTestFixture, Matchmaking_QuickGame) {
    auto player1 = createAndAuthPlayer("Seeker1");
    auto player2 = createAndAuthPlayer("Seeker2");
    auto player3 = createAndAuthPlayer("Seeker3");

    lobby->addToQueue(player1);
    EXPECT_EQ(lobby->getQueueSize(), 1);
    EXPECT_TRUE(lobby->isInQueue(player1->getGuid()));

    auto noMatch = lobby->tryMatchPlayers();
    EXPECT_FALSE(noMatch.has_value());

    lobby->addToQueue(player2);
    EXPECT_EQ(lobby->getQueueSize(), 2);

    auto match = lobby->tryMatchPlayers();
    ASSERT_TRUE(match.has_value());

    auto game = *match;
    EXPECT_EQ(game->getState(), GameState::Placing);
    EXPECT_EQ(lobby->getQueueSize(), 0);

    EXPECT_FALSE(lobby->isInQueue(player1->getGuid()));
    EXPECT_FALSE(lobby->isInQueue(player2->getGuid()));

    lobby->addToQueue(player3);
    auto noMatch2 = lobby->tryMatchPlayers();
    EXPECT_FALSE(noMatch2.has_value());
    EXPECT_EQ(lobby->getQueueSize(), 1);
}

TEST_F(IntegrationTestFixture, Matchmaking_MultipleGames) {
    std::vector<std::shared_ptr<Player>> players;
    for (int i = 0; i < 6; ++i) {
        players.push_back(createAndAuthPlayer("Player" + std::to_string(i)));
        lobby->addToQueue(players.back());
    }

    EXPECT_EQ(lobby->getQueueSize(), 6);

    std::vector<std::shared_ptr<Game>> games;
    while (auto match = lobby->tryMatchPlayers()) {
        games.push_back(*match);
    }

    EXPECT_EQ(games.size(), 3);
    EXPECT_EQ(lobby->getQueueSize(), 0);

    for (const auto& game : games) {
        EXPECT_EQ(game->getState(), GameState::Placing);
    }
}

TEST_F(IntegrationTestFixture, Invite_AcceptAndPlay) {
    auto player1 = createAndAuthPlayer("Inviter");
    auto player2 = createAndAuthPlayer("Invitee");

    std::string inviteId = lobby->sendInvite(player1->getGuid(), player2->getGuid());
    EXPECT_FALSE(inviteId.empty());

    auto invite = lobby->getInvite(inviteId);
    ASSERT_TRUE(invite.has_value());
    EXPECT_EQ(invite->from_guid, player1->getGuid());
    EXPECT_EQ(invite->to_guid, player2->getGuid());

    auto game = lobby->acceptInvite(inviteId, player1, player2);
    ASSERT_TRUE(game.has_value());
    EXPECT_EQ((*game)->getState(), GameState::Placing);
    EXPECT_TRUE((*game)->isPlayer(player1->getGuid()));
    EXPECT_TRUE((*game)->isPlayer(player2->getGuid()));

    EXPECT_FALSE(lobby->getInvite(inviteId).has_value());
}

TEST_F(IntegrationTestFixture, Invite_Decline) {
    auto player1 = createAndAuthPlayer("Inviter");
    auto player2 = createAndAuthPlayer("Invitee");

    std::string inviteId = lobby->sendInvite(player1->getGuid(), player2->getGuid());
    EXPECT_FALSE(inviteId.empty());

    bool declined = lobby->declineInvite(inviteId);
    EXPECT_TRUE(declined);

    EXPECT_FALSE(lobby->getInvite(inviteId).has_value());
}

TEST_F(IntegrationTestFixture, Invite_Duplicate_Fails) {
    auto player1 = createAndAuthPlayer("Inviter");
    auto player2 = createAndAuthPlayer("Invitee");

    std::string inviteId1 = lobby->sendInvite(player1->getGuid(), player2->getGuid());
    EXPECT_FALSE(inviteId1.empty());

    std::string inviteId2 = lobby->sendInvite(player1->getGuid(), player2->getGuid());
    EXPECT_TRUE(inviteId2.empty());
}

TEST_F(IntegrationTestFixture, Friends_AddAndCheck) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");
    auto player3 = createAndAuthPlayer("Player3");

    EXPECT_TRUE(player1->getFriends().addFriend(player2->getGuid()));
    EXPECT_TRUE(player1->getFriends().addFriend(player3->getGuid()));

    EXPECT_TRUE(player1->getFriends().isFriend(player2->getGuid()));
    EXPECT_TRUE(player1->getFriends().isFriend(player3->getGuid()));
    EXPECT_FALSE(player1->getFriends().isFriend(player1->getGuid()));

    EXPECT_FALSE(player2->getFriends().isFriend(player1->getGuid()));

    auto friends = player1->getFriends().getFriends();
    EXPECT_EQ(friends.size(), 2);

    EXPECT_TRUE(player1->getFriends().removeFriend(player2->getGuid()));
    EXPECT_FALSE(player1->getFriends().isFriend(player2->getGuid()));
    EXPECT_TRUE(player1->getFriends().isFriend(player3->getGuid()));
}

TEST_F(IntegrationTestFixture, Spectator_WatchesGame) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");
    auto spectator = createAndAuthPlayer("Spectator");

    spectator->getFriends().addFriend(player1->getGuid());

    auto game = gameManager->createGame(player1, player2);
    ASSERT_NE(game, nullptr);

    boost::asio::io_context io_context;
    auto session = std::make_shared<Session>(io_context);

    game->addSpectator(session);
    EXPECT_EQ(game->getSpectatorCount(), 1);

    game->placeShips(player1->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();

    auto fog1 = game->getBoardFogOfWar(player1->getGuid());
    auto fog2 = game->getBoardFogOfWar(player2->getGuid());

    EXPECT_TRUE(fog1.empty());
    EXPECT_TRUE(fog2.empty());

    std::string shooter = game->getCurrentTurnGuid();
    game->shoot(shooter, 0, 0);

    std::string opponent = game->getOpponentGuid(shooter);
    auto fogAfterShot = game->getBoardFogOfWar(opponent);
    EXPECT_EQ(fogAfterShot.size(), 1);

    game->removeSpectator(session);
    EXPECT_EQ(game->getSpectatorCount(), 0);
}

TEST_F(IntegrationTestFixture, Surrender_EndsGame) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    auto game = gameManager->createGame(player1, player2);
    game->placeShips(player1->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();

    EXPECT_EQ(game->getState(), GameState::Battle);

    game->surrender(player1->getGuid());

    EXPECT_TRUE(game->isFinished());
    EXPECT_EQ(game->getWinnerGuid(), player2->getGuid());
    EXPECT_EQ(game->getGameOverReason(), "surrender");
}

TEST_F(IntegrationTestFixture, Disconnect_TechnicalDefeat) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    auto game = gameManager->createGame(player1, player2);
    game->placeShips(player1->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();

    game->handleDisconnect(player2->getGuid());

    EXPECT_TRUE(game->isFinished());
    EXPECT_EQ(game->getWinnerGuid(), player1->getGuid());
    EXPECT_EQ(game->getGameOverReason(), "disconnect");
}

TEST_F(IntegrationTestFixture, Reconnect_WithSameGuid) {
    std::string guid;
    {
        auto player = playerManager->createPlayer("OriginalPlayer");
        guid = player->getGuid();
        player->setStatus(PlayerStatus::Online);

        player->getStats().recordWin();
        player->getStats().recordWin();
        player->getStats().recordLoss();

        player->getFriends().addFriend("some-friend-guid");

        playerManager->setPlayerOffline(guid);
        EXPECT_EQ(player->getStatus(), PlayerStatus::Offline);
    }

    auto reconnectedPlayer = playerManager->getPlayer(guid);
    ASSERT_NE(reconnectedPlayer, nullptr);

    reconnectedPlayer->setNickname("ReconnectedPlayer");
    playerManager->setPlayerOnline(guid, nullptr);

    EXPECT_EQ(reconnectedPlayer->getStatus(), PlayerStatus::Online);
    EXPECT_EQ(reconnectedPlayer->getNickname(), "ReconnectedPlayer");
    EXPECT_EQ(reconnectedPlayer->getStats().getWins(), 2);
    EXPECT_EQ(reconnectedPlayer->getStats().getLosses(), 1);
    EXPECT_TRUE(reconnectedPlayer->getFriends().isFriend("some-friend-guid"));
}

TEST_F(IntegrationTestFixture, Stats_UpdateAfterGame) {
    auto player1 = createAndAuthPlayer("Winner");
    auto player2 = createAndAuthPlayer("Loser");

    EXPECT_EQ(player1->getStats().getGamesPlayed(), 0);
    EXPECT_EQ(player2->getStats().getGamesPlayed(), 0);

    auto game = gameManager->createGame(player1, player2);
    game->placeShips(player1->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();

    int shotsFired = 0;
    int shotsHit = 0;

    std::string shooter = player1->getGuid();
    int opponentShotX = 0, opponentShotY = 0;

    for (int y = 0; y < 10 && !game->isFinished(); ++y) {
        for (int x = 0; x < 10 && !game->isFinished(); ++x) {
            std::string currentPlayer = game->getCurrentTurnGuid();

            if (currentPlayer != shooter) {
                while (opponentShotY < 10) {
                    auto oppResult = game->shoot(currentPlayer, opponentShotX, opponentShotY);
                    opponentShotX++;
                    if (opponentShotX >= 10) {
                        opponentShotX = 0;
                        opponentShotY++;
                    }
                    if (oppResult.result == ShotResult::Miss) {
                        break;
                    }
                    if (oppResult.result != ShotResult::AlreadyShot) {
                        break;
                    }
                }
                if (game->getCurrentTurnGuid() != shooter) {
                    continue;
                }
            }

            auto result = game->shoot(shooter, x, y);
            if (result.result == ShotResult::AlreadyShot) {
                continue;
            }
            ++shotsFired;
            if (result.result == ShotResult::Hit || result.result == ShotResult::Kill) {
                ++shotsHit;
            }
            player1->getStats().recordShot(result.result == ShotResult::Hit ||
                                           result.result == ShotResult::Kill);
        }
    }

    EXPECT_TRUE(game->isFinished());

    player1->getStats().recordWin();
    player2->getStats().recordLoss();

    EXPECT_EQ(player1->getStats().getWins(), 1);
    EXPECT_EQ(player2->getStats().getLosses(), 1);
    EXPECT_GT(player1->getStats().getShotsFired(), 0);
}

TEST_F(IntegrationTestFixture, PlayerStatus_Transitions) {
    auto player = createAndAuthPlayer("TestPlayer");

    EXPECT_EQ(player->getStatus(), PlayerStatus::Online);
    EXPECT_TRUE(player->isOnline());
    EXPECT_FALSE(player->getCurrentGameId().has_value());

    auto player2 = createAndAuthPlayer("Opponent");
    auto game = gameManager->createGame(player, player2);

    EXPECT_EQ(player->getStatus(), PlayerStatus::InGame);
    EXPECT_TRUE(player->getCurrentGameId().has_value());
    EXPECT_EQ(player->getCurrentGameId().value(), game->getId());

    game->placeShips(player->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();
    game->surrender(player->getGuid());

    player->setStatus(PlayerStatus::Online);
    player->setCurrentGameId(std::nullopt);

    EXPECT_EQ(player->getStatus(), PlayerStatus::Online);
    EXPECT_FALSE(player->getCurrentGameId().has_value());

    playerManager->setPlayerOffline(player->getGuid());

    EXPECT_EQ(player->getStatus(), PlayerStatus::Offline);
    EXPECT_FALSE(player->isOnline());
}

TEST_F(IntegrationTestFixture, MultipleGames_Concurrent) {
    const int numGames = 5;
    std::vector<std::shared_ptr<Game>> games;
    std::vector<std::shared_ptr<Player>> players;

    for (int i = 0; i < numGames * 2; ++i) {
        players.push_back(createAndAuthPlayer("Player" + std::to_string(i)));
    }

    for (int i = 0; i < numGames; ++i) {
        auto game = gameManager->createGame(players[i * 2], players[i * 2 + 1]);
        games.push_back(game);
    }

    EXPECT_EQ(gameManager->getActiveGameCount(), numGames);

    for (auto& game : games) {
        game->placeShips(game->getPlayer1Guid(), createValidPlacement());
        game->placeShips(game->getPlayer2Guid(), createAlternativePlacement());
        game->startBattle();
    }

    for (auto& game : games) {
        EXPECT_EQ(game->getState(), GameState::Battle);
    }

    for (auto& game : games) {
        game->surrender(game->getPlayer1Guid());
        EXPECT_TRUE(game->isFinished());
    }
}

TEST_F(IntegrationTestFixture, QueueCleanup_OnDisconnect) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");
    auto player3 = createAndAuthPlayer("Player3");

    lobby->addToQueue(player1);
    lobby->addToQueue(player2);
    lobby->addToQueue(player3);

    EXPECT_EQ(lobby->getQueueSize(), 3);

    lobby->handlePlayerDisconnect(player2->getGuid());

    EXPECT_EQ(lobby->getQueueSize(), 2);
    EXPECT_TRUE(lobby->isInQueue(player1->getGuid()));
    EXPECT_FALSE(lobby->isInQueue(player2->getGuid()));
    EXPECT_TRUE(lobby->isInQueue(player3->getGuid()));
}

TEST_F(IntegrationTestFixture, InviteCleanup_OnDisconnect) {
    auto player1 = createAndAuthPlayer("Inviter");
    auto player2 = createAndAuthPlayer("Invitee");
    auto player3 = createAndAuthPlayer("ThirdParty");

    lobby->sendInvite(player1->getGuid(), player2->getGuid());
    lobby->sendInvite(player3->getGuid(), player1->getGuid());

    auto invitesFor1 = lobby->getInvitesForPlayer(player1->getGuid());
    auto invitesFrom1 = lobby->getInvitesFromPlayer(player1->getGuid());

    EXPECT_EQ(invitesFor1.size(), 1);
    EXPECT_EQ(invitesFrom1.size(), 1);

    lobby->handlePlayerDisconnect(player1->getGuid());

    auto invitesFor1After = lobby->getInvitesForPlayer(player1->getGuid());
    auto invitesFrom1After = lobby->getInvitesFromPlayer(player1->getGuid());

    EXPECT_EQ(invitesFor1After.size(), 0);
    EXPECT_EQ(invitesFrom1After.size(), 0);
}

TEST_F(IntegrationTestFixture, RandomPlacement_Works) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    auto game = gameManager->createGame(player1, player2);

    game->placeShipsRandomly(player1->getGuid());
    game->placeShipsRandomly(player2->getGuid());

    EXPECT_TRUE(game->isPlayerReady(player1->getGuid()));
    EXPECT_TRUE(game->isPlayerReady(player2->getGuid()));
    EXPECT_TRUE(game->areBothPlayersReady());

    game->startBattle();
    EXPECT_EQ(game->getState(), GameState::Battle);
}

TEST_F(IntegrationTestFixture, InvalidPlacement_Rejected) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    auto game = gameManager->createGame(player1, player2);

    std::vector<ShipPlacement> invalidShips = {{0, 0, 4, true}, {1, 0, 3, true}};

    EXPECT_FALSE(game->placeShips(player1->getGuid(), invalidShips));
    EXPECT_FALSE(game->isPlayerReady(player1->getGuid()));

    std::vector<ShipPlacement> overlappingShips = {
        {0, 0, 4, true}, {0, 2, 3, true}, {0, 4, 3, true}, {0, 6, 2, true}, {0, 6, 2, true},
        {6, 6, 2, true}, {0, 8, 1, true}, {2, 8, 1, true}, {4, 8, 1, true}, {6, 8, 1, true}};

    EXPECT_FALSE(game->placeShips(player1->getGuid(), overlappingShips));
}

TEST_F(IntegrationTestFixture, WrongTurn_Rejected) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    auto game = gameManager->createGame(player1, player2);
    game->placeShips(player1->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();

    std::string currentTurn = game->getCurrentTurnGuid();
    std::string notTurn = game->getOpponentGuid(currentTurn);

    auto result = game->shoot(notTurn, 5, 5);
    EXPECT_EQ(result.result, ShotResult::InvalidCoordinates);

    EXPECT_EQ(game->getCurrentTurnGuid(), currentTurn);
}

TEST_F(IntegrationTestFixture, DoubleShot_SameCell_Rejected) {
    auto player1 = createAndAuthPlayer("Player1");
    auto player2 = createAndAuthPlayer("Player2");

    auto game = gameManager->createGame(player1, player2);
    game->placeShips(player1->getGuid(), createValidPlacement());
    game->placeShips(player2->getGuid(), createAlternativePlacement());
    game->startBattle();

    std::string shooter = game->getCurrentTurnGuid();

    auto result1 = game->shoot(shooter, 9, 9);
    EXPECT_EQ(result1.result, ShotResult::Miss);

    std::string otherPlayer = game->getOpponentGuid(shooter);
    game->shoot(otherPlayer, 9, 9);

    auto result2 = game->shoot(shooter, 9, 9);
    EXPECT_EQ(result2.result, ShotResult::AlreadyShot);
}
