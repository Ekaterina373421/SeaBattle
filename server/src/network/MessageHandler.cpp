#include "MessageHandler.hpp"

#include "Server.hpp"
#include "Session.hpp"
#include "SessionManager.hpp"
#include "game/Game.hpp"
#include "game/GameManager.hpp"
#include "game/GameRules.hpp"
#include "lobby/Lobby.hpp"
#include "player/Player.hpp"
#include "player/PlayerManager.hpp"
#include "protocol/MessageBuilder.hpp"
#include "protocol/MessageParser.hpp"
#include "utils/Logger.hpp"

namespace seabattle {

MessageHandler::MessageHandler(Server& server) : server_(server) {
    server_.setGameOverCallback([this](std::shared_ptr<Game> g) { this->notifyGameOver(g); });
}

void MessageHandler::handleMessage(std::shared_ptr<Session> session,
                                   const std::string& rawMessage) {
    Logger::debug("Получено сообщение от сессии #{}: {}", session->getId(),
                  rawMessage.substr(0, 100));

    auto parseResult = MessageParser::parse(rawMessage);

    if (std::holds_alternative<ParseError>(parseResult)) {
        auto error = std::get<ParseError>(parseResult);
        Logger::warn("Ошибка парсинга от сессии #{}: {}", session->getId(), error.message);
        session->send(MessageBuilder::buildError(-1, error.message));
        return;
    }

    auto msg = std::get<ParsedMessage>(parseResult);
    const auto& action = msg.action;
    const auto& payload = msg.payload;

    Logger::debug("Обработка действия '{}' от сессии #{}", action, session->getId());

    if (action == protocol::action::AUTH) {
        handleAuth(session, payload);
        return;
    }

    auto player = session->getPlayer();
    if (!player) {
        Logger::warn("Неавторизованный запрос от сессии #{}", session->getId());
        sendError(session, protocol::error_code::GUID_NOT_FOUND);
        return;
    }

    if (action == protocol::action::GET_PLAYERS) {
        handleGetPlayers(session);
    } else if (action == protocol::action::ADD_FRIEND) {
        handleAddFriend(session, payload);
    } else if (action == protocol::action::REMOVE_FRIEND) {
        handleRemoveFriend(session, payload);
    } else if (action == protocol::action::GET_FRIENDS) {
        handleGetFriends(session);
    } else if (action == protocol::action::GET_STATS) {
        handleGetStats(session, payload);
    } else if (action == protocol::action::FIND_GAME) {
        handleFindGame(session);
    } else if (action == protocol::action::CANCEL_SEARCH) {
        handleCancelSearch(session);
    } else if (action == protocol::action::INVITE) {
        handleInvite(session, payload);
    } else if (action == protocol::action::INVITE_RESPONSE) {
        handleInviteResponse(session, payload);
    } else if (action == protocol::action::SPECTATE) {
        handleSpectate(session, payload);
    } else if (action == protocol::action::PLACE_SHIPS) {
        handlePlaceShips(session, payload);
    } else if (action == protocol::action::SHOOT) {
        handleShoot(session, payload);
    } else if (action == protocol::action::SURRENDER) {
        handleSurrender(session);
    } else {
        Logger::warn("Неизвестное действие '{}' от игрока {}", action, player->getGuid());
        sendError(session, -1);
    }
}

// MessageHandler.cpp, метод handleAuth целиком
void MessageHandler::handleAuth(std::shared_ptr<Session> session, const nlohmann::json& payload) {
    if (session->getPlayer()) {
        session->send(MessageBuilder::buildAuthError(protocol::error_code::INVALID_NICKNAME,
                                                     "Уже авторизован"));
        return;
    }

    auto nickname = MessageParser::extractString(payload, "nickname");
    auto existingGuid = MessageParser::extractString(payload, "guid");

    if (!nickname.has_value() || nickname->empty()) {
        session->send(MessageBuilder::buildAuthError(protocol::error_code::INVALID_NICKNAME,
                                                     "Никнейм не указан"));
        return;
    }

    if (!GameRules::isValidNickname(*nickname)) {
        session->send(MessageBuilder::buildAuthError(
            protocol::error_code::INVALID_NICKNAME,
            protocol::errorMessage(protocol::error_code::INVALID_NICKNAME)));
        return;
    }

    // --- Новая логика ---
    std::shared_ptr<Player> player;

    // Сначала пытаемся обработать реконнект по GUID
    if (existingGuid.has_value() && !existingGuid->empty()) {
        player = server_.getPlayerManager()->getPlayer(*existingGuid);
        if (player && player->getNickname() != *nickname) {
            // Если ник при реконнектекте изменился, обновляем его
            player->setNickname(*nickname);
        }
    }

    if (!player) {
        bool wasCreated = false;
        player = server_.getPlayerManager()->findOrCreatePlayer(*nickname, wasCreated);

        if (!wasCreated && player->isOnline()) {
            // Игрок с таким ником уже существует и онлайн. Это не реконнект. Ошибка.
            session->send(MessageBuilder::buildAuthError(protocol::error_code::INVALID_NICKNAME,
                                                         "Никнейм уже занят"));
            return;
        }
        if (wasCreated) {
            Logger::info("Новый игрок {} ({})", player->getNickname(), player->getGuid());
        }
    }

    // Если мы здесь, игрок либо новый, либо оффлайн, либо это реконнект
    if (player->isOnline() && player->getSession().lock() != session) {
        // Кто-то пытается зайти под активным аккаунтом. Отклоняем.
        session->send(MessageBuilder::buildAuthError(protocol::error_code::INVALID_NICKNAME,
                                                     "Аккаунт уже используется"));
        return;
    }

    Logger::info("Игрок {} ({}) авторизован", player->getNickname(), player->getGuid());

    session->setPlayer(player);
    player->setSession(session);
    server_.getPlayerManager()->setPlayerOnline(player->getGuid(), session);
    server_.getSessionManager()->bindPlayerToSession(player->getGuid(), session);

    session->send(
        MessageBuilder::buildAuthResponse(true, player->getGuid(), player->getNickname()));

    auto statusMsg =
        MessageBuilder::buildPlayerStatusChanged(player->getGuid(), PlayerStatus::Online);
    server_.getSessionManager()->broadcastExcept(statusMsg, session->getId());
}

void MessageHandler::handleGetPlayers(std::shared_ptr<Session> session) {
    auto players = server_.getPlayerManager()->getOnlinePlayers();
    session->send(MessageBuilder::buildPlayerList(players));
}

void MessageHandler::handleAddFriend(std::shared_ptr<Session> session,
                                     const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto friendGuid = MessageParser::extractString(payload, "guid");
    if (!friendGuid.has_value() || friendGuid->empty()) {
        friendGuid = MessageParser::extractString(payload, "friend_guid");
    }

    if (!friendGuid.has_value() || friendGuid->empty()) {
        sendError(session, protocol::error_code::PLAYER_NOT_FOUND);
        return;
    }

    if (*friendGuid == player->getGuid()) {
        sendError(session, protocol::error_code::CANNOT_ADD_SELF);
        return;
    }

    if (!server_.getPlayerManager()->playerExists(*friendGuid)) {
        sendError(session, protocol::error_code::PLAYER_NOT_FOUND);
        return;
    }

    if (!player->getFriends().addFriend(*friendGuid)) {
        sendError(session, protocol::error_code::FRIEND_ALREADY_ADDED);
        return;
    }

    Logger::debug("Игрок {} добавил в друзья {}", player->getGuid(), *friendGuid);
    session->send(MessageBuilder::buildSuccess(protocol::action::ADD_FRIEND));
}

void MessageHandler::handleRemoveFriend(std::shared_ptr<Session> session,
                                        const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto friendGuid = MessageParser::extractString(payload, "friend_guid");

    if (!friendGuid.has_value() || friendGuid->empty()) {
        sendError(session, protocol::error_code::PLAYER_NOT_FOUND);
        return;
    }

    player->getFriends().removeFriend(*friendGuid);
    Logger::debug("Игрок {} удалил из друзей {}", player->getGuid(), *friendGuid);
    session->send(MessageBuilder::buildSuccess(protocol::action::REMOVE_FRIEND));
}

void MessageHandler::handleGetFriends(std::shared_ptr<Session> session) {
    auto player = session->getPlayer();
    auto friendGuids = player->getFriends().getFriends();

    std::vector<FriendInfo> friends;
    friends.reserve(friendGuids.size());

    for (const auto& guid : friendGuids) {
        auto friendPlayer = server_.getPlayerManager()->getPlayer(guid);
        if (friendPlayer) {
            friends.push_back({guid, friendPlayer->getNickname(), friendPlayer->getStatus()});
        }
    }

    session->send(MessageBuilder::buildFriendList(friends));
}

void MessageHandler::handleGetStats(std::shared_ptr<Session> session,
                                    const nlohmann::json& payload) {
    auto playerGuid = MessageParser::extractString(payload, "guid");
    if (!playerGuid.has_value() || playerGuid->empty()) {
        playerGuid = MessageParser::extractString(payload, "player_guid");
    }

    std::shared_ptr<Player> targetPlayer;
    if (playerGuid.has_value() && !playerGuid->empty()) {
        targetPlayer = server_.getPlayerManager()->getPlayer(*playerGuid);
    } else {
        targetPlayer = session->getPlayer();
    }

    if (!targetPlayer) {
        sendError(session, protocol::error_code::PLAYER_NOT_FOUND);
        return;
    }

    session->send(MessageBuilder::buildStatsResponse(
        targetPlayer->getGuid(), targetPlayer->getNickname(), targetPlayer->getStats()));
}

void MessageHandler::handleFindGame(std::shared_ptr<Session> session) {
    auto player = session->getPlayer();

    if (player->getStatus() == PlayerStatus::InGame) {
        sendError(session, protocol::error_code::PLAYER_IN_GAME);
        return;
    }

    server_.getLobby()->addToQueue(player);
    Logger::info("Игрок {} добавлен в очередь поиска", player->getNickname());

    session->send(MessageBuilder::buildFindGameResponse(true, server_.getLobby()->getQueueSize()));

    checkMatchmaking();
}

void MessageHandler::handleCancelSearch(std::shared_ptr<Session> session) {
    auto player = session->getPlayer();
    server_.getLobby()->removeFromQueue(player->getGuid());
    Logger::debug("Игрок {} отменил поиск игры", player->getNickname());
    session->send(MessageBuilder::buildSuccess(protocol::action::CANCEL_SEARCH));
}

void MessageHandler::handleInvite(std::shared_ptr<Session> session, const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto targetGuid = MessageParser::extractString(payload, "guid");
    if (!targetGuid.has_value() || targetGuid->empty()) {
        targetGuid = MessageParser::extractString(payload, "target_guid");
    }

    if (!targetGuid.has_value() || targetGuid->empty()) {
        sendError(session, protocol::error_code::PLAYER_NOT_FOUND);
        return;
    }

    auto targetPlayer = server_.getPlayerManager()->getPlayer(*targetGuid);
    if (!targetPlayer) {
        sendError(session, protocol::error_code::PLAYER_NOT_FOUND);
        return;
    }

    if (targetPlayer->getStatus() == PlayerStatus::Offline) {
        sendError(session, protocol::error_code::PLAYER_NOT_ONLINE);
        return;
    }

    if (targetPlayer->getStatus() == PlayerStatus::InGame) {
        sendError(session, protocol::error_code::PLAYER_IN_GAME);
        return;
    }

    auto inviteId = server_.getLobby()->sendInvite(player->getGuid(), *targetGuid);
    if (inviteId.empty()) {
        sendError(session, protocol::error_code::INVITE_NOT_FOUND);
        return;
    }

    Logger::info("Игрок {} пригласил {} в игру", player->getNickname(),
                 targetPlayer->getNickname());

    session->send(MessageBuilder::buildInviteResponse(true, inviteId));

    auto inviteMsg =
        MessageBuilder::buildInviteReceived(inviteId, player->getGuid(), player->getNickname());
    sendToPlayer(*targetGuid, inviteMsg);
}

void MessageHandler::handleInviteResponse(std::shared_ptr<Session> session,
                                          const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto inviteId = MessageParser::extractString(payload, "invite_id");
    auto accepted = MessageParser::extractBool(payload, "accept");
    if (!accepted.has_value()) {
        accepted = MessageParser::extractBool(payload, "accepted");
    }

    if (!inviteId.has_value() || !accepted.has_value()) {
        sendError(session, protocol::error_code::INVITE_NOT_FOUND);
        return;
    }

    auto invite = server_.getLobby()->getInvite(*inviteId);
    if (!invite.has_value()) {
        sendError(session, protocol::error_code::INVITE_EXPIRED);
        return;
    }

    if (!*accepted) {
        server_.getLobby()->declineInvite(*inviteId);
        Logger::debug("Игрок {} отклонил приглашение", player->getNickname());
        session->send(MessageBuilder::buildInviteResponse(true, *inviteId));
        return;
    }

    auto fromPlayer = server_.getPlayerManager()->getPlayer(invite->from_guid);
    if (!fromPlayer || fromPlayer->getStatus() != PlayerStatus::Online) {
        sendError(session, protocol::error_code::PLAYER_NOT_ONLINE);
        server_.getLobby()->declineInvite(*inviteId);
        return;
    }

    auto game = server_.getLobby()->acceptInvite(*inviteId, fromPlayer, player);
    if (!game.has_value()) {
        sendError(session, protocol::error_code::PLAYER_NOT_ONLINE);
        return;
    }

    Logger::info("Игра {} начата: {} vs {}", (*game)->getId(), fromPlayer->getNickname(),
                 player->getNickname());

    notifyGameStart(*game);
}

void MessageHandler::handleSpectate(std::shared_ptr<Session> session,
                                    const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto gameId = MessageParser::extractString(payload, "game_id");

    if (!gameId.has_value() || gameId->empty()) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    auto game = server_.getGameManager()->getGame(*gameId);
    if (!game) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    bool isFriend = player->getFriends().isFriend(game->getPlayer1Guid()) ||
                    player->getFriends().isFriend(game->getPlayer2Guid());

    if (!isFriend) {
        sendError(session, protocol::error_code::SPECTATE_FRIENDS_ONLY);
        return;
    }

    game->addSpectator(session);
    Logger::debug("Игрок {} наблюдает за игрой {}", player->getNickname(), *gameId);

    auto fog1 = game->getBoardFogOfWar(game->getPlayer1Guid());
    auto fog2 = game->getBoardFogOfWar(game->getPlayer2Guid());

    session->send(MessageBuilder::buildSpectateResponse(
        true, game->getPlayer1Guid(), game->getPlayerNickname(game->getPlayer1Guid()),
        game->getPlayer2Guid(), game->getPlayerNickname(game->getPlayer2Guid()),
        game->getCurrentTurnGuid(), fog1, fog2));
}

void MessageHandler::handlePlaceShips(std::shared_ptr<Session> session,
                                      const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto game = server_.getGameManager()->getGameByPlayer(player->getGuid());

    if (!game) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    if (game->getState() != GameState::Placing) {
        session->send(MessageBuilder::buildPlaceShipsResponse(false, "Расстановка уже завершена"));
        return;
    }

    auto ships = MessageParser::extractShips(payload);
    if (!ships.has_value()) {
        session->send(MessageBuilder::buildPlaceShipsResponse(false, "Невалидный формат кораблей"));
        return;
    }

    if (!game->placeShips(player->getGuid(), *ships)) {
        session->send(
            MessageBuilder::buildPlaceShipsResponse(false, "Невалидная расстановка кораблей"));
        return;
    }

    Logger::debug("Игрок {} расставил корабли", player->getNickname());
    session->send(MessageBuilder::buildPlaceShipsResponse(true));

    auto readyMsg = MessageBuilder::buildPlayerReady(player->getGuid());
    sendToPlayer(game->getOpponentGuid(player->getGuid()), readyMsg);

    if (game->areBothPlayersReady()) {
        game->startBattle();
        Logger::info("Игра {} перешла в фазу боя", game->getId());
        notifyBattleStart(game);
    }
}

void MessageHandler::handleShoot(std::shared_ptr<Session> session, const nlohmann::json& payload) {
    auto player = session->getPlayer();
    auto game = server_.getGameManager()->getGameByPlayer(player->getGuid());

    if (!game) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    if (game->getState() != GameState::Battle) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    if (!game->isPlayerTurn(player->getGuid())) {
        sendError(session, protocol::error_code::NOT_YOUR_TURN);
        return;
    }

    auto coords = MessageParser::extractCoordinates(payload);
    if (!coords.has_value()) {
        sendError(session, protocol::error_code::CELL_ALREADY_ATTACKED);
        return;
    }

    auto result = game->shoot(player->getGuid(), coords->first, coords->second);

    if (result.result == ShotResult::AlreadyShot) {
        sendError(session, protocol::error_code::CELL_ALREADY_ATTACKED);
        return;
    }

    if (result.result == ShotResult::InvalidCoordinates) {
        sendError(session, protocol::error_code::CELL_ALREADY_ATTACKED);
        return;
    }

    bool isHit = (result.result == ShotResult::Hit || result.result == ShotResult::Kill);
    player->getStats().recordShot(isHit);

    Logger::debug("Игрок {} стрелял в ({}, {}): {}", player->getNickname(), coords->first,
                  coords->second, isHit ? "попадание" : "промах");

    session->send(MessageBuilder::buildShotResult(result));

    bool opponentTurn = (result.result == ShotResult::Miss);
    auto opponentMsg = MessageBuilder::buildOpponentShot(
        coords->first, coords->second, result.result, opponentTurn, result.killed_ship_cells,
        result.surrounding_misses);
    sendToPlayer(game->getOpponentGuid(player->getGuid()), opponentMsg);

    auto spectateMsg = MessageBuilder::buildSpectateUpdate(
        player->getGuid(), coords->first, coords->second, result.result, result.next_turn_guid);
    game->notifySpectators(spectateMsg);

    if (game->isFinished()) {
        notifyGameOver(game);
    }
}

void MessageHandler::handleSurrender(std::shared_ptr<Session> session) {
    auto player = session->getPlayer();
    auto game = server_.getGameManager()->getGameByPlayer(player->getGuid());

    if (!game) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    if (game->isFinished()) {
        sendError(session, protocol::error_code::GAME_NOT_FOUND);
        return;
    }

    Logger::info("Игрок {} сдался в игре {}", player->getNickname(), game->getId());
    game->surrender(player->getGuid());
    notifyGameOver(game);
}

void MessageHandler::sendError(std::shared_ptr<Session> session, int errorCode) {
    session->send(MessageBuilder::buildError(errorCode, protocol::errorMessage(errorCode)));
}

void MessageHandler::sendToPlayer(const std::string& playerGuid, const std::string& message) {
    auto session = server_.getSessionManager()->getSessionByPlayerGuid(playerGuid);
    if (session && session->isOpen()) {
        session->send(message);
    }
}

void MessageHandler::notifyGameStart(std::shared_ptr<Game> game) {
    auto msg1 = MessageBuilder::buildGameStarted(game->getId(), game->getPlayer2Guid(),
                                                 game->getPlayerNickname(game->getPlayer2Guid()));
    sendToPlayer(game->getPlayer1Guid(), msg1);

    auto msg2 = MessageBuilder::buildGameStarted(game->getId(), game->getPlayer1Guid(),
                                                 game->getPlayerNickname(game->getPlayer1Guid()));
    sendToPlayer(game->getPlayer2Guid(), msg2);

    auto statusMsg1 = MessageBuilder::buildPlayerStatusChanged(game->getPlayer1Guid(),
                                                               PlayerStatus::InGame, game->getId());
    auto statusMsg2 = MessageBuilder::buildPlayerStatusChanged(game->getPlayer2Guid(),
                                                               PlayerStatus::InGame, game->getId());

    server_.getSessionManager()->broadcast(statusMsg1);
    server_.getSessionManager()->broadcast(statusMsg2);
}

void MessageHandler::notifyBattleStart(std::shared_ptr<Game> game) {
    auto battleMsg = MessageBuilder::buildBattleStart(game->getCurrentTurnGuid());
    sendToPlayer(game->getPlayer1Guid(), battleMsg);
    sendToPlayer(game->getPlayer2Guid(), battleMsg);
}

void MessageHandler::notifyGameOver(std::shared_ptr<Game> game) {
    auto winnerGuid = game->getWinnerGuid().value_or("");
    auto loserGuid = game->getOpponentGuid(winnerGuid);

    auto winnerPlayer = server_.getPlayerManager()->getPlayer(winnerGuid);
    auto loserPlayer = server_.getPlayerManager()->getPlayer(loserGuid);

    if (winnerPlayer) {
        winnerPlayer->getStats().recordWin();
        winnerPlayer->setStatus(PlayerStatus::Online);
        winnerPlayer->setCurrentGameId(std::nullopt);
    }

    if (loserPlayer) {
        loserPlayer->getStats().recordLoss();
        loserPlayer->setStatus(PlayerStatus::Online);
        loserPlayer->setCurrentGameId(std::nullopt);
    }

    auto board1 = game->getBoardFullState(game->getPlayer1Guid());
    auto board2 = game->getBoardFullState(game->getPlayer2Guid());

    auto gameOverMsg =
        MessageBuilder::buildGameOver(winnerGuid, game->getGameOverReason(), board1, board2);

    sendToPlayer(game->getPlayer1Guid(), gameOverMsg);
    sendToPlayer(game->getPlayer2Guid(), gameOverMsg);
    game->notifySpectators(gameOverMsg);

    if (winnerPlayer) {
        auto statusMsg = MessageBuilder::buildPlayerStatusChanged(winnerGuid, PlayerStatus::Online);
        server_.getSessionManager()->broadcast(statusMsg);
    }

    if (loserPlayer) {
        auto statusMsg = MessageBuilder::buildPlayerStatusChanged(loserGuid, PlayerStatus::Online);
        server_.getSessionManager()->broadcast(statusMsg);
    }

    Logger::info("Игра {} завершена. Победитель: {}", game->getId(),
                 winnerPlayer ? winnerPlayer->getNickname() : "неизвестен");
}

void MessageHandler::checkMatchmaking() {
    while (true) {
        auto game = server_.getLobby()->tryMatchPlayers();
        if (!game.has_value()) {
            break;
        }

        Logger::info("Матчмейкинг: создана игра {} ({} vs {})", (*game)->getId(),
                     (*game)->getPlayerNickname((*game)->getPlayer1Guid()),
                     (*game)->getPlayerNickname((*game)->getPlayer2Guid()));

        notifyGameStart(*game);
    }
}

}  // namespace seabattle