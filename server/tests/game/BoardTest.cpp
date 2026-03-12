#include <gtest/gtest.h>

#include "game/Board.hpp"

using namespace seabattle;

class BoardTest : public ::testing::Test {
   protected:
    Board board;
};

TEST_F(BoardTest, Constructor_EmptyBoard) {
    for (int y = 0; y < Board::SIZE; ++y) {
        for (int x = 0; x < Board::SIZE; ++x) {
            EXPECT_EQ(board.getCell(x, y), CellState::Empty);
        }
    }
    EXPECT_EQ(board.getShipCount(), 0);
}

TEST_F(BoardTest, PlaceShip_Valid_ReturnsTrue) {
    EXPECT_TRUE(board.placeShip(0, 0, 4, true));
    EXPECT_EQ(board.getShipCount(), 1);

    EXPECT_EQ(board.getCell(0, 0), CellState::Ship);
    EXPECT_EQ(board.getCell(1, 0), CellState::Ship);
    EXPECT_EQ(board.getCell(2, 0), CellState::Ship);
    EXPECT_EQ(board.getCell(3, 0), CellState::Ship);
}

TEST_F(BoardTest, PlaceShip_Vertical_ReturnsTrue) {
    EXPECT_TRUE(board.placeShip(5, 2, 3, false));

    EXPECT_EQ(board.getCell(5, 2), CellState::Ship);
    EXPECT_EQ(board.getCell(5, 3), CellState::Ship);
    EXPECT_EQ(board.getCell(5, 4), CellState::Ship);
}

TEST_F(BoardTest, PlaceShip_OutOfBounds_Horizontal_ReturnsFalse) {
    EXPECT_FALSE(board.placeShip(8, 0, 4, true));
    EXPECT_EQ(board.getShipCount(), 0);
}

TEST_F(BoardTest, PlaceShip_OutOfBounds_Vertical_ReturnsFalse) {
    EXPECT_FALSE(board.placeShip(0, 9, 3, false));
    EXPECT_EQ(board.getShipCount(), 0);
}

TEST_F(BoardTest, PlaceShip_NegativeCoordinates_ReturnsFalse) {
    EXPECT_FALSE(board.placeShip(-1, 0, 2, true));
    EXPECT_FALSE(board.placeShip(0, -1, 2, true));
}

TEST_F(BoardTest, PlaceShip_Overlap_ReturnsFalse) {
    EXPECT_TRUE(board.placeShip(2, 2, 3, true));
    EXPECT_FALSE(board.placeShip(3, 2, 2, true));
    EXPECT_EQ(board.getShipCount(), 1);
}

TEST_F(BoardTest, PlaceShip_TouchingHorizontally_ReturnsFalse) {
    EXPECT_TRUE(board.placeShip(0, 0, 2, true));
    EXPECT_FALSE(board.placeShip(2, 0, 2, true));
}

TEST_F(BoardTest, PlaceShip_TouchingVertically_ReturnsFalse) {
    EXPECT_TRUE(board.placeShip(0, 0, 2, false));
    EXPECT_FALSE(board.placeShip(0, 2, 2, false));
}

TEST_F(BoardTest, PlaceShip_DiagonalTouch_ReturnsFalse) {
    EXPECT_TRUE(board.placeShip(0, 0, 2, true));
    EXPECT_FALSE(board.placeShip(2, 1, 2, true));
}

TEST_F(BoardTest, PlaceShip_WithGap_ReturnsTrue) {
    EXPECT_TRUE(board.placeShip(0, 0, 2, true));
    EXPECT_TRUE(board.placeShip(4, 0, 2, true));
    EXPECT_EQ(board.getShipCount(), 2);
}

TEST_F(BoardTest, PlaceShip_UsingPlacement_ReturnsTrue) {
    ShipPlacement placement{3, 3, 3, false};
    EXPECT_TRUE(board.placeShip(placement));
    EXPECT_EQ(board.getShipCount(), 1);
}

TEST_F(BoardTest, Shoot_Empty_ReturnsMiss) {
    EXPECT_EQ(board.shoot(5, 5), ShotResult::Miss);
    EXPECT_EQ(board.getCell(5, 5), CellState::Miss);
}

TEST_F(BoardTest, Shoot_Ship_ReturnsHit) {
    board.placeShip(0, 0, 3, true);

    EXPECT_EQ(board.shoot(0, 0), ShotResult::Hit);
    EXPECT_EQ(board.getCell(0, 0), CellState::Hit);
}

TEST_F(BoardTest, Shoot_LastCell_ReturnsKill) {
    board.placeShip(0, 0, 2, true);

    EXPECT_EQ(board.shoot(0, 0), ShotResult::Hit);
    EXPECT_EQ(board.shoot(1, 0), ShotResult::Kill);
}

TEST_F(BoardTest, Shoot_Kill_MarksSurroundingAsMiss) {
    board.placeShip(2, 2, 2, true);

    board.shoot(2, 2);
    board.shoot(3, 2);

    EXPECT_EQ(board.getCell(1, 1), CellState::Miss);
    EXPECT_EQ(board.getCell(1, 2), CellState::Miss);
    EXPECT_EQ(board.getCell(1, 3), CellState::Miss);
    EXPECT_EQ(board.getCell(4, 1), CellState::Miss);
    EXPECT_EQ(board.getCell(4, 2), CellState::Miss);
    EXPECT_EQ(board.getCell(4, 3), CellState::Miss);
}

TEST_F(BoardTest, Shoot_AlreadyHit_ReturnsAlreadyShot) {
    board.placeShip(0, 0, 3, true);

    board.shoot(0, 0);
    EXPECT_EQ(board.shoot(0, 0), ShotResult::AlreadyShot);
}

TEST_F(BoardTest, Shoot_AlreadyMiss_ReturnsAlreadyShot) {
    board.shoot(5, 5);
    EXPECT_EQ(board.shoot(5, 5), ShotResult::AlreadyShot);
}

TEST_F(BoardTest, Shoot_InvalidCoordinates_ReturnsInvalid) {
    EXPECT_EQ(board.shoot(-1, 0), ShotResult::InvalidCoordinates);
    EXPECT_EQ(board.shoot(0, -1), ShotResult::InvalidCoordinates);
    EXPECT_EQ(board.shoot(10, 0), ShotResult::InvalidCoordinates);
    EXPECT_EQ(board.shoot(0, 10), ShotResult::InvalidCoordinates);
}

TEST_F(BoardTest, AreAllShipsSunk_NoShips_ReturnsFalse) {
    EXPECT_FALSE(board.areAllShipsSunk());
}

TEST_F(BoardTest, AreAllShipsSunk_NotAllSunk_ReturnsFalse) {
    board.placeShip(0, 0, 2, true);
    board.placeShip(5, 5, 2, true);

    board.shoot(0, 0);
    board.shoot(1, 0);

    EXPECT_FALSE(board.areAllShipsSunk());
}

TEST_F(BoardTest, AreAllShipsSunk_AllSunk_ReturnsTrue) {
    board.placeShip(0, 0, 2, true);
    board.placeShip(5, 5, 1, true);

    board.shoot(0, 0);
    board.shoot(1, 0);
    board.shoot(5, 5);

    EXPECT_TRUE(board.areAllShipsSunk());
}

TEST_F(BoardTest, GetFogOfWar_HidesShips) {
    board.placeShip(0, 0, 3, true);
    board.shoot(0, 0);
    board.shoot(5, 5);

    auto fog = board.getFogOfWar();

    EXPECT_EQ(fog.size(), 2);

    bool foundHit = false;
    bool foundMiss = false;
    for (const auto& cell : fog) {
        if (cell.x == 0 && cell.y == 0 && cell.state == CellState::Hit) {
            foundHit = true;
        }
        if (cell.x == 5 && cell.y == 5 && cell.state == CellState::Miss) {
            foundMiss = true;
        }
    }
    EXPECT_TRUE(foundHit);
    EXPECT_TRUE(foundMiss);
}

TEST_F(BoardTest, GetShipAt_Exists_ReturnsShip) {
    board.placeShip(2, 2, 3, true);

    const Ship* ship = board.getShipAt(3, 2);

    ASSERT_NE(ship, nullptr);
    EXPECT_EQ(ship->getSize(), 3);
}

TEST_F(BoardTest, GetShipAt_NotExists_ReturnsNull) {
    board.placeShip(2, 2, 3, true);

    EXPECT_EQ(board.getShipAt(0, 0), nullptr);
}

TEST_F(BoardTest, Clear_RemovesAllShips) {
    board.placeShip(0, 0, 3, true);
    board.placeShip(5, 5, 2, true);

    board.clear();

    EXPECT_EQ(board.getShipCount(), 0);
    EXPECT_EQ(board.getCell(0, 0), CellState::Empty);
}

TEST_F(BoardTest, PlaceShipsRandomly_PlacesAllShips) {
    board.placeShipsRandomly();

    EXPECT_EQ(board.getShipCount(), GameConfig::TOTAL_SHIPS);

    int shipCells = 0;
    for (int y = 0; y < Board::SIZE; ++y) {
        for (int x = 0; x < Board::SIZE; ++x) {
            if (board.getCell(x, y) == CellState::Ship) {
                ++shipCells;
            }
        }
    }
    EXPECT_EQ(shipCells, GameConfig::TOTAL_SHIP_CELLS);
}

TEST_F(BoardTest, PlaceShipsRandomly_ShipsDoNotTouch) {
    board.placeShipsRandomly();

    for (int y = 0; y < Board::SIZE; ++y) {
        for (int x = 0; x < Board::SIZE; ++x) {
            if (board.getCell(x, y) == CellState::Ship) {
                const Ship* ship = board.getShipAt(x, y);
                ASSERT_NE(ship, nullptr);

                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;

                        int nx = x + dx;
                        int ny = y + dy;

                        if (nx >= 0 && nx < Board::SIZE && ny >= 0 && ny < Board::SIZE) {
                            if (board.getCell(nx, ny) == CellState::Ship) {
                                const Ship* neighbor = board.getShipAt(nx, ny);
                                EXPECT_EQ(ship, neighbor)
                                    << "Соседние клетки должны принадлежать одному кораблю";
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_F(BoardTest, CanPlaceShip_Valid_ReturnsTrue) {
    EXPECT_TRUE(board.canPlaceShip(0, 0, 4, true));
}

TEST_F(BoardTest, CanPlaceShip_AfterPlacement_ReturnsFalse) {
    board.placeShip(0, 0, 3, true);
    EXPECT_FALSE(board.canPlaceShip(1, 0, 2, true));
    EXPECT_FALSE(board.canPlaceShip(0, 1, 2, true));
}

TEST_F(BoardTest, GetSurroundingCells_ReturnsCorrectCells) {
    board.placeShip(3, 3, 2, true);

    const Ship* ship = board.getShipAt(3, 3);
    ASSERT_NE(ship, nullptr);

    auto surrounding = board.getSurroundingCells(*ship);

    EXPECT_GE(surrounding.size(), 8u);
}