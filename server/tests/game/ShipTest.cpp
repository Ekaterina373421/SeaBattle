#include <gtest/gtest.h>

#include "game/Ship.hpp"

using namespace seabattle;

class ShipTest : public ::testing::Test {
   protected:
    void SetUp() override {}
};

TEST_F(ShipTest, Constructor_Horizontal_CreatesCorrectCells) {
    Ship ship(2, 3, 4, true);

    ASSERT_EQ(ship.getSize(), 4);

    const auto& cells = ship.getCells();
    EXPECT_EQ(cells[0], std::make_pair(2, 3));
    EXPECT_EQ(cells[1], std::make_pair(3, 3));
    EXPECT_EQ(cells[2], std::make_pair(4, 3));
    EXPECT_EQ(cells[3], std::make_pair(5, 3));
}

TEST_F(ShipTest, Constructor_Vertical_CreatesCorrectCells) {
    Ship ship(1, 2, 3, false);

    ASSERT_EQ(ship.getSize(), 3);

    const auto& cells = ship.getCells();
    EXPECT_EQ(cells[0], std::make_pair(1, 2));
    EXPECT_EQ(cells[1], std::make_pair(1, 3));
    EXPECT_EQ(cells[2], std::make_pair(1, 4));
}

TEST_F(ShipTest, Constructor_SingleCell_CreatesOneCell) {
    Ship ship(5, 5, 1, true);

    ASSERT_EQ(ship.getSize(), 1);
    EXPECT_EQ(ship.getCells()[0], std::make_pair(5, 5));
}

TEST_F(ShipTest, Hit_ValidCell_MarksAsHit) {
    Ship ship(0, 0, 3, true);

    EXPECT_EQ(ship.getHitCount(), 0);

    ship.hit(1, 0);

    EXPECT_EQ(ship.getHitCount(), 1);
}

TEST_F(ShipTest, Hit_InvalidCell_DoesNothing) {
    Ship ship(0, 0, 3, true);

    ship.hit(5, 5);

    EXPECT_EQ(ship.getHitCount(), 0);
}

TEST_F(ShipTest, Hit_SameCell_CountsOnce) {
    Ship ship(0, 0, 3, true);

    ship.hit(0, 0);
    ship.hit(0, 0);
    ship.hit(0, 0);

    EXPECT_EQ(ship.getHitCount(), 1);
}

TEST_F(ShipTest, IsSunk_NotAllHit_ReturnsFalse) {
    Ship ship(0, 0, 3, true);

    ship.hit(0, 0);
    ship.hit(1, 0);

    EXPECT_FALSE(ship.isSunk());
}

TEST_F(ShipTest, IsSunk_AllHit_ReturnsTrue) {
    Ship ship(0, 0, 3, true);

    ship.hit(0, 0);
    ship.hit(1, 0);
    ship.hit(2, 0);

    EXPECT_TRUE(ship.isSunk());
}

TEST_F(ShipTest, IsSunk_SingleCellShip_HitOnce_ReturnsTrue) {
    Ship ship(5, 5, 1, true);

    ship.hit(5, 5);

    EXPECT_TRUE(ship.isSunk());
}

TEST_F(ShipTest, ContainsCell_CellInShip_ReturnsTrue) {
    Ship ship(2, 3, 4, true);

    EXPECT_TRUE(ship.containsCell(2, 3));
    EXPECT_TRUE(ship.containsCell(3, 3));
    EXPECT_TRUE(ship.containsCell(4, 3));
    EXPECT_TRUE(ship.containsCell(5, 3));
}

TEST_F(ShipTest, ContainsCell_CellNotInShip_ReturnsFalse) {
    Ship ship(2, 3, 4, true);

    EXPECT_FALSE(ship.containsCell(0, 0));
    EXPECT_FALSE(ship.containsCell(2, 4));
    EXPECT_FALSE(ship.containsCell(6, 3));
    EXPECT_FALSE(ship.containsCell(1, 3));
}
