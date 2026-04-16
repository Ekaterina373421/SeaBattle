#include "Session.hpp"

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif
#include <cstring>

#include "player/Player.hpp"
#include "protocol/MessageParser.hpp"

namespace seabattle {

Session::Session(boost::asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
    read_buffer_.resize(HEADER_SIZE);
}

Session::Session(boost::asio::io_context& io_context) : socket_(io_context) {
    read_buffer_.resize(HEADER_SIZE);
}

void Session::start() {
    doReadHeader();
}

void Session::doReadHeader() {
    auto self = shared_from_this();
    read_buffer_.resize(HEADER_SIZE);

    boost::asio::async_read(
        socket_, boost::asio::buffer(read_buffer_),
        [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
            if (ec) {
                handleError(ec);
                return;
            }

            uint32_t netLength;
            std::memcpy(&netLength, read_buffer_.data(), sizeof(netLength));
            uint32_t bodyLength = ntohl(netLength);

            if (bodyLength > protocol::MAX_MESSAGE_SIZE) {
                handleError(boost::asio::error::message_size);
                return;
            }

            doReadBody(bodyLength);
        });
}

void Session::doReadBody(uint32_t length) {
    auto self = shared_from_this();
    read_buffer_.resize(length);

    boost::asio::async_read(
        socket_, boost::asio::buffer(read_buffer_),
        [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
            if (ec) {
                handleError(ec);
                return;
            }

            std::string message(reinterpret_cast<char*>(read_buffer_.data()), read_buffer_.size());

            if (message_callback_) {
                message_callback_(self, message);
            }

            doReadHeader();
        });
}

void Session::send(const std::string& message) {
    if (!is_open_)
        return;

    auto packed = MessageParser::packMessage(message);

    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        sent_messages_.push_back(message);
        write_queue_.push(std::move(packed));

        if (is_writing_)
            return;
        is_writing_ = true;
    }

    doWrite();
}

void Session::doWrite() {
    auto self = shared_from_this();

    std::vector<uint8_t> data;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty()) {
            is_writing_ = false;
            return;
        }
        data = std::move(write_queue_.front());
        write_queue_.pop();
    }

    boost::asio::async_write(
        socket_, boost::asio::buffer(data),
        [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
            if (ec) {
                handleError(ec);
                return;
            }
            doWrite();
        });
}

void Session::close() {
    bool expected = true;
    if (!is_open_.compare_exchange_strong(expected, false)) {
        return;
    }

    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

void Session::handleError(const boost::system::error_code& ec) {
    if (!is_open_)
        return;

    close();

    if (error_callback_) {
        error_callback_(shared_from_this(), ec);
    }
}

std::shared_ptr<Player> Session::getPlayer() const {
    std::lock_guard<std::mutex> lock(player_mutex_);
    return player_.lock();
}

void Session::setPlayer(std::shared_ptr<Player> player) {
    std::lock_guard<std::mutex> lock(player_mutex_);
    player_ = player;
}

bool Session::isOpen() const {
    return is_open_;
}

void Session::setMessageCallback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void Session::setErrorCallback(ErrorCallback callback) {
    error_callback_ = std::move(callback);
}

const std::vector<std::string>& Session::getSentMessages() const {
    return sent_messages_;
}

void Session::clearSentMessages() {
    std::lock_guard<std::mutex> lock(write_mutex_);
    sent_messages_.clear();
}

}  // namespace seabattle
