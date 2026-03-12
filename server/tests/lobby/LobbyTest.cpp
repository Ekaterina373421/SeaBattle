#include <gtest/gtest.h>

#include "game/GameManager.hpp"
#include "lobby/Lobby.hpp"
#include "player/Player.hpp"

using namespace seabattle;

class LobbyTest : public ::testing::Test {
   protected:
    std::shared_ptr<GameManager> gameManager;
    std::unique_ptr<Lobby> lobby;

    void SetUp() override {
        gameManager = std::make_shared<GameManager>();
        lobby = std::make_unique<Lobby>(gameManager);
    }

    std::shared_ptr<Player> createOnlinePlayer(const std::string& guid) {
        auto player = std::make_shared<Player>(guid, "Player_" + guid);
        player->setStatus(PlayerStatus::Online);
        return player;
    }
};

TEST_F(LobbyTest, AddToQueue_OnlinePlayer_AddsToQueue) {
    auto player = createOnlinePlayer("guid-1");

    lobby->addToQueue(player);

    EXPECT_TRUE(lobby->isInQueue("guid-1"));
    EXPECT_EQ(lobby->getQueueSize(), 1);
}

TEST_F(LobbyTest, AddToQueue_OfflinePlayer_DoesNotAdd) {
    auto player = std::make_shared<Player>("guid-1", "Player1");
    player->setStatus(PlayerStatus::Offline);

    lobby->addToQueue(player);

    EXPECT_FALSE(lobby->isInQueue("guid-1"));
}

TEST_F(LobbyTest, RemoveFromQueue_RemovesPlayer) {
    auto player = createOnlinePlayer("guid-1");
    lobby->addToQueue(player);

    lobby->removeFromQueue("guid-1");

    EXPECT_FALSE(lobby->isInQueue("guid-1"));
}

TEST_F(LobbyTest, TryMatchPlayers_TwoPlayers_CreatesGame) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = createOnlinePlayer("guid-2");
    lobby->addToQueue(player1);
    lobby->addToQueue(player2);

    auto game = lobby->tryMatchPlayers();

    ASSERT_TRUE(game.has_value());
    EXPECT_FALSE((*game)->getId().empty());
    EXPECT_EQ(gameManager->getActiveGameCount(), 1);
}

TEST_F(LobbyTest, TryMatchPlayers_OnePlayer_ReturnsEmpty) {
    auto player = createOnlinePlayer("guid-1");
    lobby->addToQueue(player);

    auto game = lobby->tryMatchPlayers();

    EXPECT_FALSE(game.has_value());
}

TEST_F(LobbyTest, SendInvite_CreatesInvite) {
    std::string inviteId = lobby->sendInvite("from-guid", "to-guid");

    EXPECT_FALSE(inviteId.empty());
    EXPECT_TRUE(lobby->hasActiveInvite("from-guid", "to-guid"));
}

TEST_F(LobbyTest, AcceptInvite_CreatesGame) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = createOnlinePlayer("guid-2");
    std::string inviteId = lobby->sendInvite("guid-1", "guid-2");

    auto game = lobby->acceptInvite(inviteId, player1, player2);

    ASSERT_TRUE(game.has_value());
    EXPECT_EQ(gameManager->getActiveGameCount(), 1);
}

TEST_F(LobbyTest, AcceptInvite_RemovesPlayersFromQueue) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = createOnlinePlayer("guid-2");
    lobby->addToQueue(player1);
    lobby->addToQueue(player2);
    std::string inviteId = lobby->sendInvite("guid-1", "guid-2");

    lobby->acceptInvite(inviteId, player1, player2);

    EXPECT_FALSE(lobby->isInQueue("guid-1"));
    EXPECT_FALSE(lobby->isInQueue("guid-2"));
}

TEST_F(LobbyTest, AcceptInvite_InvalidId_ReturnsEmpty) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = createOnlinePlayer("guid-2");

    auto game = lobby->acceptInvite("invalid-id", player1, player2);

    EXPECT_FALSE(game.has_value());
}

TEST_F(LobbyTest, AcceptInvite_OfflinePlayer_ReturnsEmpty) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = std::make_shared<Player>("guid-2", "Player2");
    player2->setStatus(PlayerStatus::Offline);
    std::string inviteId = lobby->sendInvite("guid-1", "guid-2");

    auto game = lobby->acceptInvite(inviteId, player1, player2);

    EXPECT_FALSE(game.has_value());
}

TEST_F(LobbyTest, DeclineInvite_RemovesInvite) {
    std::string inviteId = lobby->sendInvite("from-guid", "to-guid");

    bool result = lobby->declineInvite(inviteId);

    EXPECT_TRUE(result);
    EXPECT_FALSE(lobby->hasActiveInvite("from-guid", "to-guid"));
}

TEST_F(LobbyTest, CancelInvite_ByCreator_RemovesInvite) {
    std::string inviteId = lobby->sendInvite("from-guid", "to-guid");

    bool result = lobby->cancelInvite(inviteId, "from-guid");

    EXPECT_TRUE(result);
    EXPECT_FALSE(lobby->hasActiveInvite("from-guid", "to-guid"));
}

TEST_F(LobbyTest, GetInvitesForPlayer_ReturnsCorrect) {
    lobby->sendInvite("from-1", "target");
    lobby->sendInvite("from-2", "target");
    lobby->sendInvite("from-3", "other");

    auto invites = lobby->getInvitesForPlayer("target");

    EXPECT_EQ(invites.size(), 2);
}

TEST_F(LobbyTest, HandlePlayerDisconnect_RemovesFromQueueAndInvites) {
    auto player = createOnlinePlayer("guid-1");
    lobby->addToQueue(player);
    lobby->sendInvite("guid-1", "guid-2");
    lobby->sendInvite("guid-3", "guid-1");

    lobby->handlePlayerDisconnect("guid-1");

    EXPECT_FALSE(lobby->isInQueue("guid-1"));
    EXPECT_FALSE(lobby->hasActiveInvite("guid-1", "guid-2"));
    EXPECT_EQ(lobby->getInvitesForPlayer("guid-1").size(), 0);
}
