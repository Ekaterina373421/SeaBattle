#include "GameManager.hpp"

#include "player/Player.hpp"
#include "utils/GuidGenerator.hpp"

namespace seabattle {

std::shared_ptr<Game> GameManager::createGame(std::shared_ptr<Player> player1,
                                              std::shared_ptr<Player> player2) {
    std::string gameId = GuidGenerator::generate();

    auto game = std::make_shared<Game>(gameId, player1, player2);

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        games_[gameId] = game;
        player_to_game_[player1->getGuid()] = gameId;
        player_to_game_[player2->getGuid()] = gameId;
    }

    player1->setCurrentGameId(gameId);
    player1->setStatus(PlayerStatus::InGame);

    player2->setCurrentGameId(gameId);
    player2->setStatus(PlayerStatus::InGame);

    return game;
}

std::shared_ptr<Game> GameManager::getGame(const std::string& gameId) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = games_.find(gameId);
    if (it != games_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Game> GameManager::getGameByPlayer(const std::string& playerGuid) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = player_to_game_.find(playerGuid);
    if (it != player_to_game_.end()) {
        auto gameIt = games_.find(it->second);
        if (gameIt != games_.end()) {
            return gameIt->second;
        }
    }
    return nullptr;
}

void GameManager::removeGame(const std::string& gameId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = games_.find(gameId);
    if (it != games_.end()) {
        auto& game = it->second;
        player_to_game_.erase(game->getPlayer1Guid());
        player_to_game_.erase(game->getPlayer2Guid());
        games_.erase(it);
    }
}

bool GameManager::addSpectator(const std::string& gameId, std::weak_ptr<Session> session) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = games_.find(gameId);
    if (it != games_.end()) {
        it->second->addSpectator(session);
        return true;
    }
    return false;
}

void GameManager::removeSpectator(const std::string& gameId, std::weak_ptr<Session> session) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = games_.find(gameId);
    if (it != games_.end()) {
        it->second->removeSpectator(session);
    }
}

std::vector<std::string> GameManager::getActiveGameIds() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(games_.size());
    for (const auto& [id, game] : games_) {
        ids.push_back(id);
    }
    return ids;
}

size_t GameManager::getActiveGameCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return games_.size();
}

}  // namespace seabattle
