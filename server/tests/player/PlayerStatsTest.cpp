#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "player/PlayerStats.hpp"

using namespace seabattle;

class PlayerStatsTest : public ::testing::Test {
   protected:
    PlayerStats stats;
};

TEST_F(PlayerStatsTest, Initial_AllZeros) {
    EXPECT_EQ(stats.getGamesPlayed(), 0);
    EXPECT_EQ(stats.getWins(), 0);
    EXPECT_EQ(stats.getLosses(), 0);
    EXPECT_EQ(stats.getShotsFired(), 0);
    EXPECT_EQ(stats.getShotsHit(), 0);
    EXPECT_DOUBLE_EQ(stats.getWinrate(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getAccuracy(), 0.0);
}

TEST_F(PlayerStatsTest, RecordWin_IncrementsWinsAndGames) {
    stats.recordWin();

    EXPECT_EQ(stats.getWins(), 1);
    EXPECT_EQ(stats.getGamesPlayed(), 1);
    EXPECT_EQ(stats.getLosses(), 0);
}

TEST_F(PlayerStatsTest, RecordLoss_IncrementsLossesAndGames) {
    stats.recordLoss();

    EXPECT_EQ(stats.getLosses(), 1);
    EXPECT_EQ(stats.getGamesPlayed(), 1);
    EXPECT_EQ(stats.getWins(), 0);
}

TEST_F(PlayerStatsTest, RecordShot_Hit_IncrementsBoth) {
    stats.recordShot(true);

    EXPECT_EQ(stats.getShotsFired(), 1);
    EXPECT_EQ(stats.getShotsHit(), 1);
}

TEST_F(PlayerStatsTest, RecordShot_Miss_IncrementsOnlyFired) {
    stats.recordShot(false);

    EXPECT_EQ(stats.getShotsFired(), 1);
    EXPECT_EQ(stats.getShotsHit(), 0);
}

TEST_F(PlayerStatsTest, GetWinrate_Correct) {
    stats.recordWin();
    stats.recordWin();
    stats.recordLoss();

    EXPECT_NEAR(stats.getWinrate(), 66.666, 0.01);
}

TEST_F(PlayerStatsTest, GetWinrate_NoGames_ReturnsZero) {
    EXPECT_DOUBLE_EQ(stats.getWinrate(), 0.0);
}

TEST_F(PlayerStatsTest, GetWinrate_AllWins_Returns100) {
    stats.recordWin();
    stats.recordWin();
    stats.recordWin();

    EXPECT_DOUBLE_EQ(stats.getWinrate(), 100.0);
}

TEST_F(PlayerStatsTest, GetAccuracy_Correct) {
    stats.recordShot(true);
    stats.recordShot(true);
    stats.recordShot(false);
    stats.recordShot(false);

    EXPECT_DOUBLE_EQ(stats.getAccuracy(), 50.0);
}

TEST_F(PlayerStatsTest, GetAccuracy_NoShots_ReturnsZero) {
    EXPECT_DOUBLE_EQ(stats.getAccuracy(), 0.0);
}

TEST_F(PlayerStatsTest, GetAccuracy_AllHits_Returns100) {
    stats.recordShot(true);
    stats.recordShot(true);
    stats.recordShot(true);

    EXPECT_DOUBLE_EQ(stats.getAccuracy(), 100.0);
}

TEST_F(PlayerStatsTest, ToJson_CorrectFormat) {
    stats.recordWin();
    stats.recordLoss();
    stats.recordShot(true);
    stats.recordShot(false);

    auto json = stats.toJson();

    EXPECT_EQ(json["games_played"], 2);
    EXPECT_EQ(json["wins"], 1);
    EXPECT_EQ(json["losses"], 1);
    EXPECT_DOUBLE_EQ(json["winrate"].get<double>(), 50.0);
    EXPECT_EQ(json["shots_fired"], 2);
    EXPECT_EQ(json["shots_hit"], 1);
    EXPECT_DOUBLE_EQ(json["accuracy"].get<double>(), 50.0);
}

TEST_F(PlayerStatsTest, ThreadSafety_ConcurrentRecords) {
    const int numThreads = 10;
    const int operationsPerThread = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                stats.recordWin();
                stats.recordShot(true);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(stats.getWins(), numThreads * operationsPerThread);
    EXPECT_EQ(stats.getShotsHit(), numThreads * operationsPerThread);
}
