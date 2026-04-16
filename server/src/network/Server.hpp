#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace seabattle {

class Session;
class SessionManager;
class PlayerManager;
class GameManager;
class Lobby;
class Game;

class Server {
   public:
    using MessageHandler = std::function<void(std::shared_ptr<Session>, const std::string&)>;

    Server(uint16_t port, size_t threadCount = 4);
    ~Server();

    void run();
    void stop();

    void setMessageHandler(MessageHandler handler);

    void setGameOverCallback(std::function<void(std::shared_ptr<Game>)> callback) {
        game_over_callback_ = std::move(callback);
    }

    std::shared_ptr<SessionManager> getSessionManager() {
        return session_manager_;
    }
    std::shared_ptr<PlayerManager> getPlayerManager() {
        return player_manager_;
    }
    std::shared_ptr<GameManager> getGameManager() {
        return game_manager_;
    }
    std::shared_ptr<Lobby> getLobby() {
        return lobby_;
    }

    boost::asio::io_context& getIoContext() {
        return io_context_;
    }

    size_t getThreadCount() const {
        return thread_count_;
    }
    uint16_t getPort() const {
        return port_;
    }
    bool isRunning() const {
        return running_;
    }

   private:
    void doAccept();
    void onSessionError(std::shared_ptr<Session> session, const boost::system::error_code& ec);
    void onPlayerDisconnect(const std::string& playerGuid);

    uint16_t port_;
    size_t thread_count_;
    std::atomic<bool> running_{false};

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;

    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<PlayerManager> player_manager_;
    std::shared_ptr<GameManager> game_manager_;
    std::shared_ptr<Lobby> lobby_;

    MessageHandler message_handler_;
    std::function<void(std::shared_ptr<Game>)> game_over_callback_;
};

}  // namespace seabattle