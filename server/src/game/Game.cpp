#include "Game.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

#include "network/Session.hpp"
#include "player/Player.hpp"

namespace seabattle {

Game::Game(const std::string& id, std::shared_ptr<Player> player1, std::shared_ptr<Player> player2)
    : id_(id) {
    player1_.guid = player1->getGuid();
    player1_.nickname = player1->getNickname();
    player1_.board = std::make_unique<Board>();

    player2_.guid = player2->getGuid();
    player2_.nickname = player2->getNickname();
    player2_.board = std::make_unique<Board>();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    current_turn_guid_ = dis(gen) == 0 ? player1_.guid : player2_.guid;
}

std::string Game::getId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return id_;
}

GameState Game::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::string Game::getCurrentTurnGuid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_turn_guid_;
}

std::string Game::getPlayer1Guid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return player1_.guid;
}

std::string Game::getPlayer2Guid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return player2_.guid;
}

std::string Game::getPlayerNickname(const std::string& guid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (guid == player1_.guid)
        return player1_.nickname;
    if (guid == player2_.guid)
        return player2_.nickname;
    return "";
}

std::string Game::getOpponentGuid(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (playerGuid == player1_.guid)
        return player2_.guid;
    if (playerGuid == player2_.guid)
        return player1_.guid;
    return "";
}

bool Game::isPlayer(const std::string& guid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return guid == player1_.guid || guid == player2_.guid;
}

bool Game::isPlayerTurn(const std::string& guid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_turn_guid_ == guid;
}

bool Game::isPlayerReady(const std::string& guid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (guid == player1_.guid)
        return player1_.ready;
    if (guid == player2_.guid)
        return player2_.ready;
    return false;
}

bool Game::areBothPlayersReady() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return player1_.ready && player2_.ready;
}

GamePlayer& Game::getGamePlayer(const std::string& guid) {
    if (guid == player1_.guid)
        return player1_;
    if (guid == player2_.guid)
        return player2_;
    throw std::runtime_error("Игрок не найден в игре");
}

const GamePlayer& Game::getGamePlayer(const std::string& guid) const {
    if (guid == player1_.guid)
        return player1_;
    if (guid == player2_.guid)
        return player2_;
    throw std::runtime_error("Игрок не найден в игре");
}

GamePlayer& Game::getOpponentGamePlayer(const std::string& guid) {
    if (guid == player1_.guid)
        return player2_;
    if (guid == player2_.guid)
        return player1_;
    throw std::runtime_error("Игрок не найден в игре");
}

bool Game::placeShips(const std::string& playerGuid, const std::vector<ShipPlacement>& ships) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != GameState::Placing) {
        return false;
    }

    auto validation = GameRules::validatePlacement(ships);
    if (!validation.valid) {
        return false;
    }

    GamePlayer& player = getGamePlayer(playerGuid);
    player.board->clear();

    for (const auto& ship : ships) {
        if (!player.board->placeShip(ship)) {
            player.board->clear();
            return false;
        }
    }

    player.ready = true;
    return true;
}

void Game::placeShipsRandomly(const std::string& playerGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != GameState::Placing) {
        return;
    }

    GamePlayer& player = getGamePlayer(playerGuid);
    player.board->placeShipsRandomly();
    player.ready = true;
}

void Game::startBattle() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == GameState::Placing && player1_.ready && player2_.ready) {
        state_ = GameState::Battle;
    }
}

ShotResultInfo Game::shoot(const std::string& shooterGuid, int x, int y) {
    std::lock_guard<std::mutex> lock(mutex_);

    ShotResultInfo info;
    info.x = x;
    info.y = y;

    if (state_ != GameState::Battle) {
        info.result = ShotResult::InvalidCoordinates;
        return info;
    }

    if (current_turn_guid_ != shooterGuid) {
        info.result = ShotResult::InvalidCoordinates;
        return info;
    }

    GamePlayer& opponent = getOpponentGamePlayer(shooterGuid);
    ShotResult result = opponent.board->shoot(x, y);
    info.result = result;

    if (result == ShotResult::Miss) {
        switchTurn();
    }

    if (result == ShotResult::Kill) {
        const Ship* ship = opponent.board->getShipAt(x, y);
        if (ship) {
            info.killed_ship_cells = ship->getCells();
            info.surrounding_misses = opponent.board->getSurroundingCells(*ship);
        }

        if (opponent.board->areAllShipsSunk()) {
            finishGame(shooterGuid, "all_ships_sunk");
        }
    }

    info.next_turn_guid = current_turn_guid_;
    return info;
}

void Game::switchTurn() {
    if (current_turn_guid_ == player1_.guid) {
        current_turn_guid_ = player2_.guid;
    } else {
        current_turn_guid_ = player1_.guid;
    }
}

void Game::surrender(const std::string& playerGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == GameState::Finished) {
        return;
    }

    std::string winnerGuid = (playerGuid == player1_.guid) ? player2_.guid : player1_.guid;
    finishGame(winnerGuid, "surrender");
}

void Game::handleDisconnect(const std::string& playerGuid) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == GameState::Finished) {
        return;
    }

    std::string winnerGuid = (playerGuid == player1_.guid) ? player2_.guid : player1_.guid;
    finishGame(winnerGuid, "disconnect");
}

void Game::finishGame(const std::string& winnerGuid, const std::string& reason) {
    state_ = GameState::Finished;
    winner_guid_ = winnerGuid;
    game_over_reason_ = reason;
}

bool Game::isFinished() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == GameState::Finished;
}

std::optional<std::string> Game::getWinnerGuid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return winner_guid_;
}

std::string Game::getGameOverReason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return game_over_reason_;
}

void Game::addSpectator(std::weak_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanupSpectators();
    spectators_.push_back(session);
}

void Game::removeSpectator(std::weak_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sessionPtr = session.lock();
    if (!sessionPtr)
        return;

    spectators_.erase(std::remove_if(spectators_.begin(), spectators_.end(),
                                     [&sessionPtr](const std::weak_ptr<Session>& s) {
                                         auto sp = s.lock();
                                         return !sp || sp == sessionPtr;
                                     }),
                      spectators_.end());
}

void Game::notifySpectators(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanupSpectators();

    for (auto& weakSession : spectators_) {
        if (auto session = weakSession.lock()) {
            session->send(message);
        }
    }
}

void Game::cleanupSpectators() {
    spectators_.erase(std::remove_if(spectators_.begin(), spectators_.end(),
                                     [](const std::weak_ptr<Session>& s) { return s.expired(); }),
                      spectators_.end());
}

size_t Game::getSpectatorCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& s : spectators_) {
        if (!s.expired())
            ++count;
    }
    return count;
}

std::vector<CellInfo> Game::getBoardFogOfWar(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const GamePlayer& player = getGamePlayer(playerGuid);
    return player.board->getFogOfWar();
}

std::vector<CellInfo> Game::getBoardFullState(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const GamePlayer& player = getGamePlayer(playerGuid);
    return player.board->getFullState();
}

Board* Game::getPlayerBoard(const std::string& playerGuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    return getGamePlayer(playerGuid).board.get();
}

const Board* Game::getPlayerBoard(const std::string& playerGuid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return getGamePlayer(playerGuid).board.get();
}

}  // namespace seabattle