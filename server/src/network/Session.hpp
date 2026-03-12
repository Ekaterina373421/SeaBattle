#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "ISession.hpp"

namespace seabattle {

class Player;
class SessionManager;

class Session : public ISession, public std::enable_shared_from_this<Session> {
   public:
    using MessageCallback = std::function<void(std::shared_ptr<Session>, const std::string&)>;
    using ErrorCallback =
        std::function<void(std::shared_ptr<Session>, const boost::system::error_code&)>;

    explicit Session(boost::asio::io_context& io_context);
    explicit Session(boost::asio::ip::tcp::socket socket);
    ~Session() override = default;

    void start();
    void send(const std::string& message) override;
    void close() override;

    std::shared_ptr<Player> getPlayer() const override;
    void setPlayer(std::shared_ptr<Player> player) override;

    bool isOpen() const override;

    uint64_t getId() const {
        return id_;
    }
    void setId(uint64_t id) {
        id_ = id;
    }

    void setMessageCallback(MessageCallback callback);
    void setErrorCallback(ErrorCallback callback);

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    const std::vector<std::string>& getSentMessages() const;
    void clearSentMessages();

   private:
    void doReadHeader();
    void doReadBody(uint32_t length);
    void doWrite();
    void handleError(const boost::system::error_code& ec);

    boost::asio::ip::tcp::socket socket_;
    uint64_t id_ = 0;
    std::atomic<bool> is_open_{true};

    mutable std::mutex player_mutex_;
    std::weak_ptr<Player> player_;

    std::vector<uint8_t> read_buffer_;
    static constexpr size_t HEADER_SIZE = 4;

    std::mutex write_mutex_;
    std::queue<std::vector<uint8_t>> write_queue_;
    bool is_writing_ = false;

    MessageCallback message_callback_;
    ErrorCallback error_callback_;

    std::vector<std::string> sent_messages_;
};

}  // namespace seabattle