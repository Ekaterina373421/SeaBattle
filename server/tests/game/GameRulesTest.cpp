#include <gtest/gtest.h>

#include "game/GameRules.hpp"

using namespace seabattle;

class GameRulesTest : public ::testing::Test {
   protected:
    std::vector<ShipPlacement> createValidPlacement() {
        return {{0, 0, 4, true}, {0, 2, 3, true}, {0, 4, 3, true}, {0, 6, 2, true},
                {3, 6, 2, true}, {6, 6, 2, true}, {0, 8, 1, true}, {2, 8, 1, true},
                {4, 8, 1, true}, {6, 8, 1, true}};
    }
};

TEST_F(GameRulesTest, ValidatePlacement_CorrectFleet_ReturnsTrue) {
    auto placement = createValidPlacement();

    auto result = GameRules::validatePlacement(placement);

    EXPECT_TRUE(result.valid) << result.error;
}

TEST_F(GameRulesTest, ValidatePlacement_WrongTotalCount_ReturnsFalse) {
    std::vector<ShipPlacement> placement = {{0, 0, 4, true}, {0, 2, 3, true}};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_WrongShipSizes_ReturnsFalse) {
    std::vector<ShipPlacement> placement = {
        {0, 0, 4, true}, {0, 2, 4, true}, {0, 4, 3, true}, {0, 6, 2, true}, {3, 6, 2, true},
        {6, 6, 2, true}, {0, 8, 1, true}, {2, 8, 1, true}, {4, 8, 1, true}, {6, 8, 1, true}};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_ShipOutOfBounds_Horizontal_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[0] = {8, 0, 4, true};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_ShipOutOfBounds_Vertical_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[0] = {0, 8, 4, false};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_NegativeCoordinates_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[0] = {-1, 0, 4, true};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_ShipsOverlap_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[1] = {1, 0, 3, true};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_ShipsTouchHorizontally_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[1] = {4, 0, 3, true};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_ShipsTouchVertically_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[1] = {0, 1, 3, true};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, ValidatePlacement_ShipsTouchDiagonally_ReturnsFalse) {
    auto placement = createValidPlacement();
    placement[1] = {4, 1, 3, true};

    auto result = GameRules::validatePlacement(placement);

    EXPECT_FALSE(result.valid);
}

TEST_F(GameRulesTest, GetRequiredShipSizes_ReturnsCorrectSizes) {
    auto sizes = GameRules::getRequiredShipSizes();

    EXPECT_EQ(sizes.size(), static_cast<size_t>(GameConfig::TOTAL_SHIPS));

    auto count4 = static_cast<int>(std::count(sizes.begin(), sizes.end(), 4));
    auto count3 = static_cast<int>(std::count(sizes.begin(), sizes.end(), 3));
    auto count2 = static_cast<int>(std::count(sizes.begin(), sizes.end(), 2));
    auto count1 = static_cast<int>(std::count(sizes.begin(), sizes.end(), 1));

    EXPECT_EQ(count4, GameConfig::SHIP_4_COUNT);
    EXPECT_EQ(count3, GameConfig::SHIP_3_COUNT);
    EXPECT_EQ(count2, GameConfig::SHIP_2_COUNT);
    EXPECT_EQ(count1, GameConfig::SHIP_1_COUNT);
}

TEST_F(GameRulesTest, IsValidNickname_ValidLatinNickname_ReturnsTrue) {
    EXPECT_TRUE(GameRules::isValidNickname("Player1"));
    EXPECT_TRUE(GameRules::isValidNickname("John_Doe"));
    EXPECT_TRUE(GameRules::isValidNickname("abc"));
}

TEST_F(GameRulesTest, IsValidNickname_ValidCyrillicNickname_ReturnsTrue) {
    EXPECT_TRUE(GameRules::isValidNickname("Игрок1"));
    EXPECT_TRUE(GameRules::isValidNickname("Вася_Пупкин"));
}

TEST_F(GameRulesTest, IsValidNickname_TooShort_ReturnsFalse) {
    EXPECT_FALSE(GameRules::isValidNickname("ab"));
    EXPECT_FALSE(GameRules::isValidNickname(""));
}

TEST_F(GameRulesTest, IsValidNickname_TooLong_ReturnsFalse) {
    EXPECT_FALSE(GameRules::isValidNickname("VeryLongNickname123"));
}

TEST_F(GameRulesTest, IsValidNickname_WithSpaces_ReturnsFalse) {
    EXPECT_FALSE(GameRules::isValidNickname("John Doe"));
}

TEST_F(GameRulesTest, IsValidNickname_WithSpecialChars_ReturnsFalse) {
    EXPECT_FALSE(GameRules::isValidNickname("Player@1"));
    EXPECT_FALSE(GameRules::isValidNickname("Player-1"));
    EXPECT_FALSE(GameRules::isValidNickname("Player.1"));
}

TEST_F(GameRulesTest, IsValidNickname_OnlyUnderscore_ReturnsTrue) {
    EXPECT_TRUE(GameRules::isValidNickname("___"));
}

TEST_F(GameRulesTest, IsValidNickname_OnlyNumbers_ReturnsTrue) {
    EXPECT_TRUE(GameRules::isValidNickname("12345"));
}