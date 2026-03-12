#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "player/PlayerManager.hpp"

using namespace seabattle;

class PlayerManagerTest : public ::testing::Test {
   protected:
    PlayerManager manager;
};

TEST_F(PlayerManagerTest, CreatePlayer_GeneratesGuid) {
    auto player = manager.createPlayer("TestPlayer");

    ASSERT_NE(player, nullptr);
    EXPECT_FALSE(player->getGuid().empty());
    EXPECT_EQ(player->getNickname(), "TestPlayer");
}

TEST_F(PlayerManagerTest, CreatePlayer_StoresPlayer) {
    auto player = manager.createPlayer("TestPlayer");

    EXPECT_EQ(manager.getPlayerCount(), 1);
    EXPECT_TRUE(manager.playerExists(player->getGuid()));
}

TEST_F(PlayerManagerTest, CreatePlayer_InitialStatusOffline) {
    auto player = manager.createPlayer("TestPlayer");

    EXPECT_EQ(player->getStatus(), PlayerStatus::Offline);
    EXPECT_FALSE(player->isOnline());
}

TEST_F(PlayerManagerTest, GetPlayer_Exists_ReturnsPlayer) {
    auto created = manager.createPlayer("TestPlayer");

    auto found = manager.getPlayer(created->getGuid());

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getGuid(), created->getGuid());
}

TEST_F(PlayerManagerTest, GetPlayer_NotExists_ReturnsNull) {
    auto found = manager.getPlayer("non-existent-guid");

    EXPECT_EQ(found, nullptr);
}

TEST_F(PlayerManagerTest, GetPlayerByNickname_Exists_ReturnsPlayer) {
    manager.createPlayer("UniqueNickname");

    auto found = manager.getPlayerByNickname("UniqueNickname");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getNickname(), "UniqueNickname");
}

TEST_F(PlayerManagerTest, GetPlayerByNickname_NotExists_ReturnsNull) {
    auto found = manager.getPlayerByNickname("NonExistent");

    EXPECT_EQ(found, nullptr);
}

TEST_F(PlayerManagerTest, SetPlayerOnline_UpdatesStatus) {
    auto player = manager.createPlayer("TestPlayer");

    bool result = manager.setPlayerOnline(player->getGuid(), nullptr);

    EXPECT_TRUE(result);
    EXPECT_EQ(player->getStatus(), PlayerStatus::Online);
    EXPECT_TRUE(player->isOnline());
}

TEST_F(PlayerManagerTest, SetPlayerOnline_NotExists_ReturnsFalse) {
    bool result = manager.setPlayerOnline("non-existent", nullptr);

    EXPECT_FALSE(result);
}

TEST_F(PlayerManagerTest, SetPlayerOffline_UpdatesStatus) {
    auto player = manager.createPlayer("TestPlayer");
    manager.setPlayerOnline(player->getGuid(), nullptr);

    manager.setPlayerOffline(player->getGuid());

    EXPECT_EQ(player->getStatus(), PlayerStatus::Offline);
    EXPECT_FALSE(player->isOnline());
}

TEST_F(PlayerManagerTest, SetPlayerOffline_ClearsGameId) {
    auto player = manager.createPlayer("TestPlayer");
    manager.setPlayerOnline(player->getGuid(), nullptr);
    player->setCurrentGameId("game-123");

    manager.setPlayerOffline(player->getGuid());

    EXPECT_FALSE(player->getCurrentGameId().has_value());
}

TEST_F(PlayerManagerTest, GetOnlinePlayers_ReturnsOnlyOnline) {
    auto player1 = manager.createPlayer("Player1");
    auto player2 = manager.createPlayer("Player2");
    auto player3 = manager.createPlayer("Player3");

    manager.setPlayerOnline(player1->getGuid(), nullptr);
    manager.setPlayerOnline(player3->getGuid(), nullptr);

    auto online = manager.getOnlinePlayers();

    EXPECT_EQ(online.size(), 2);
}

TEST_F(PlayerManagerTest, GetAllPlayers_ReturnsAll) {
    manager.createPlayer("Player1");
    manager.createPlayer("Player2");
    manager.createPlayer("Player3");

    auto all = manager.getAllPlayers();

    EXPECT_EQ(all.size(), 3);
}

TEST_F(PlayerManagerTest, GetOnlineCount_Correct) {
    auto player1 = manager.createPlayer("Player1");
    auto player2 = manager.createPlayer("Player2");
    manager.createPlayer("Player3");

    manager.setPlayerOnline(player1->getGuid(), nullptr);
    manager.setPlayerOnline(player2->getGuid(), nullptr);

    EXPECT_EQ(manager.getOnlineCount(), 2);
}

TEST_F(PlayerManagerTest, ThreadSafety_ConcurrentCreate) {
    const int numThreads = 10;
    const int playersPerThread = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, playersPerThread]() {
            for (int j = 0; j < playersPerThread; ++j) {
                manager.createPlayer("Player_" + std::to_string(i) + "_" + std::to_string(j));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(manager.getPlayerCount(), numThreads * playersPerThread);
}

TEST_F(PlayerManagerTest, ThreadSafety_ConcurrentAccess) {
    auto player = manager.createPlayer("TestPlayer");
    std::string guid = player->getGuid();

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &guid]() {
            for (int j = 0; j < 100; ++j) {
                auto p = manager.getPlayer(guid);
                EXPECT_NE(p, nullptr);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}
