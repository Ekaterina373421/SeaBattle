#include "Server.hpp"

#include "Session.hpp"
#include "SessionManager.hpp"
#include "game/GameManager.hpp"
#include "lobby/Lobby.hpp"
#include "player/PlayerManager.hpp"
#include "utils/Logger.hpp"

namespace seabattle {

Server::Server(uint16_t port, size_t threadCount)
    : port_(port),
      thread_count_(threadCount),
      acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      session_manager_(std::make_shared<SessionManager>()),
      player_manager_(std::make_shared<PlayerManager>()),
      game_manager_(std::make_shared<GameManager>()),
      lobby_(std::make_shared<Lobby>(game_manager_)) {
    session_manager_->setDisconnectCallback(
        [this](const std::string& playerGuid) { onPlayerDisconnect(playerGuid); });

    Logger::info("Сервер инициализирован на порту {}", port);
}

Server::~Server() {
    stop();
}

void Server::run() {
    if (running_)
        return;
    running_ = true;

    doAccept();

    threads_.reserve(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        threads_.emplace_back([this]() { io_context_.run(); });
    }

    Logger::info("Сервер запущен с {} потоками", thread_count_);
}

void Server::stop() {
    if (!running_)
        return;
    running_ = false;

    io_context_.stop();

    session_manager_->closeAll();

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();

    Logger::info("Сервер остановлен");
}

void Server::doAccept() {
    acceptor_.async_accept(
        [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
            if (ec) {
                if (running_) {
                    Logger::error("Ошибка принятия соединения: {}", ec.message());
                    doAccept();
                }
                return;
            }

            auto session = std::make_shared<Session>(std::move(socket));

            session->setMessageCallback(
                [this](std::shared_ptr<Session> sess, const std::string& message) {
                    if (message_handler_) {
                        message_handler_(sess, message);
                    }
                });

            session->setErrorCallback(
                [this](std::shared_ptr<Session> sess, const boost::system::error_code& err) {
                    onSessionError(sess, err);
                });

            session_manager_->addSession(session);
            session->start();

            Logger::debug("Новое соединение, сессия #{}", session->getId());

            doAccept();
        });
}

void Server::onSessionError(std::shared_ptr<Session> session, const boost::system::error_code& ec) {
    if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
        ec == boost::asio::error::operation_aborted) {
        Logger::debug("Сессия #{} отключена", session->getId());
    } else {
        Logger::warn("Ошибка сессии #{}: {}", session->getId(), ec.message());
    }

    session_manager_->removeSession(session);
}

void Server::onPlayerDisconnect(const std::string& playerGuid) {
    Logger::info("Игрок {} отключился", playerGuid);

    lobby_->handlePlayerDisconnect(playerGuid);

    auto game = game_manager_->getGameByPlayer(playerGuid);
    if (game && !game->isFinished()) {
        game->handleDisconnect(playerGuid);
    }

    player_manager_->setPlayerOffline(playerGuid);
}

void Server::setMessageHandler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

}  // namespace seabattle