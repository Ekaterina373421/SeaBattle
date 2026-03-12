#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "lobby/Matchmaker.hpp"
#include "player/Player.hpp"

using namespace seabattle;

class MatchmakerTest : public ::testing::Test {
   protected:
    Matchmaker matchmaker;

    std::shared_ptr<Player> createOnlinePlayer(const std::string& guid) {
        auto player = std::make_shared<Player>(guid, "Player_" + guid);
        player->setStatus(PlayerStatus::Online);
        return player;
    }
};

TEST_F(MatchmakerTest, Initial_QueueEmpty) {
    EXPECT_EQ(matchmaker.getQueueSize(), 0);
}

TEST_F(MatchmakerTest, AddPlayer_IncreasesQueueSize) {
    auto player = createOnlinePlayer("guid-1");

    matchmaker.addPlayer(player);

    EXPECT_EQ(matchmaker.getQueueSize(), 1);
}

TEST_F(MatchmakerTest, AddPlayer_Nullptr_DoesNothing) {
    matchmaker.addPlayer(nullptr);

    EXPECT_EQ(matchmaker.getQueueSize(), 0);
}

TEST_F(MatchmakerTest, AddPlayer_Duplicate_DoesNotAdd) {
    auto player = createOnlinePlayer("guid-1");

    matchmaker.addPlayer(player);
    matchmaker.addPlayer(player);

    EXPECT_EQ(matchmaker.getQueueSize(), 1);
}

TEST_F(MatchmakerTest, AddPlayer_Multiple_IncreasesQueueSize) {
    auto p1 = createOnlinePlayer("guid-1");
    auto p2 = createOnlinePlayer("guid-2");
    auto p3 = createOnlinePlayer("guid-3");

    matchmaker.addPlayer(p1);
    matchmaker.addPlayer(p2);
    matchmaker.addPlayer(p3);

    EXPECT_EQ(matchmaker.getQueueSize(), 3);
}

TEST_F(MatchmakerTest, RemovePlayer_DecreasesQueueSize) {
    auto player = createOnlinePlayer("guid-1");
    matchmaker.addPlayer(player);

    matchmaker.removePlayer("guid-1");

    EXPECT_EQ(matchmaker.getQueueSize(), 0);
}

TEST_F(MatchmakerTest, RemovePlayer_NotInQueue_DoesNothing) {
    auto player = createOnlinePlayer("guid-1");
    matchmaker.addPlayer(player);

    matchmaker.removePlayer("guid-2");

    EXPECT_EQ(matchmaker.getQueueSize(), 1);
}

TEST_F(MatchmakerTest, IsInQueue_PlayerInQueue_ReturnsTrue) {
    auto player = createOnlinePlayer("guid-1");
    matchmaker.addPlayer(player);

    EXPECT_TRUE(matchmaker.isInQueue("guid-1"));
}

TEST_F(MatchmakerTest, IsInQueue_PlayerNotInQueue_ReturnsFalse) {
    EXPECT_FALSE(matchmaker.isInQueue("guid-1"));
}

TEST_F(MatchmakerTest, TryMatch_OnePlayer_ReturnsEmpty) {
    auto player = createOnlinePlayer("guid-1");
    matchmaker.addPlayer(player);

    auto result = matchmaker.tryMatch();

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(matchmaker.getQueueSize(), 1);
}

TEST_F(MatchmakerTest, TryMatch_TwoPlayers_ReturnsPair) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = createOnlinePlayer("guid-2");
    matchmaker.addPlayer(player1);
    matchmaker.addPlayer(player2);

    auto result = matchmaker.tryMatch();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first->getGuid(), "guid-1");
    EXPECT_EQ(result->second->getGuid(), "guid-2");
}

TEST_F(MatchmakerTest, TryMatch_RemovesBothFromQueue) {
    matchmaker.addPlayer(createOnlinePlayer("guid-1"));
    matchmaker.addPlayer(createOnlinePlayer("guid-2"));

    matchmaker.tryMatch();

    EXPECT_EQ(matchmaker.getQueueSize(), 0);
}

TEST_F(MatchmakerTest, TryMatch_FIFO_Order) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = createOnlinePlayer("guid-2");
    auto player3 = createOnlinePlayer("guid-3");
    matchmaker.addPlayer(player1);
    matchmaker.addPlayer(player2);
    matchmaker.addPlayer(player3);

    auto result = matchmaker.tryMatch();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first->getGuid(), "guid-1");
    EXPECT_EQ(result->second->getGuid(), "guid-2");
    EXPECT_EQ(matchmaker.getQueueSize(), 1);
    EXPECT_TRUE(matchmaker.isInQueue("guid-3"));
}

TEST_F(MatchmakerTest, TryMatch_SkipsOfflinePlayers) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = std::make_shared<Player>("guid-2", "Player2");
    player2->setStatus(PlayerStatus::Offline);
    auto player3 = createOnlinePlayer("guid-3");

    matchmaker.addPlayer(player1);
    matchmaker.addPlayer(player2);
    matchmaker.addPlayer(player3);

    auto result = matchmaker.tryMatch();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first->getGuid(), "guid-1");
    EXPECT_EQ(result->second->getGuid(), "guid-3");
}

TEST_F(MatchmakerTest, TryMatch_SkipsInGamePlayers) {
    auto player1 = createOnlinePlayer("guid-1");
    auto player2 = std::make_shared<Player>("guid-2", "Player2");
    player2->setStatus(PlayerStatus::InGame);
    auto player3 = createOnlinePlayer("guid-3");

    matchmaker.addPlayer(player1);
    matchmaker.addPlayer(player2);
    matchmaker.addPlayer(player3);

    auto result = matchmaker.tryMatch();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first->getGuid(), "guid-1");
    EXPECT_EQ(result->second->getGuid(), "guid-3");
}

TEST_F(MatchmakerTest, Clear_EmptiesQueue) {
    matchmaker.addPlayer(createOnlinePlayer("guid-1"));
    matchmaker.addPlayer(createOnlinePlayer("guid-2"));

    matchmaker.clear();

    EXPECT_EQ(matchmaker.getQueueSize(), 0);
}

TEST_F(MatchmakerTest, ExpiredWeakPtr_IsCleanedUp) {
    {
        auto player = createOnlinePlayer("guid-1");
        matchmaker.addPlayer(player);
    }

    EXPECT_EQ(matchmaker.getQueueSize(), 0);
}

TEST_F(MatchmakerTest, ThreadSafety_ConcurrentAddRemove) {
    std::vector<std::shared_ptr<Player>> players;
    for (int i = 0; i < 20; ++i) {
        players.push_back(createOnlinePlayer("guid-" + std::to_string(i)));
    }

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &players, i]() { matchmaker.addPlayer(players[i]); });
    }

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() { matchmaker.removePlayer("guid-" + std::to_string(i)); });
    }

    for (auto& t : threads) {
        t.join();
    }
}

TEST_F(MatchmakerTest, ThreadSafety_ConcurrentMatch) {
    for (int i = 0; i < 10; ++i) {
        matchmaker.addPlayer(createOnlinePlayer("guid-" + std::to_string(i)));
    }

    std::vector<std::thread> threads;
    std::atomic<int> matchCount{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &matchCount]() {
            if (matchmaker.tryMatch().has_value()) {
                ++matchCount;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_LE(matchCount, 5);
}
