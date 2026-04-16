#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace seabattle {

namespace protocol {

constexpr uint16_t DEFAULT_PORT = 12345;
constexpr int DEFAULT_THREAD_COUNT = 4;
constexpr size_t LENGTH_HEADER_SIZE = 4;
constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024;  // 1 MB

namespace message_type {
constexpr const char* REQUEST = "request";
constexpr const char* RESPONSE = "response";
constexpr const char* EVENT = "event";
}  // namespace message_type

namespace action {
// Аутентификация
constexpr const char* AUTH = "auth";

// Игроки
constexpr const char* GET_PLAYERS = "get_players";
constexpr const char* PLAYER_STATUS_CHANGED = "player_status_changed";

// Друзья
constexpr const char* ADD_FRIEND = "add_friend";
constexpr const char* REMOVE_FRIEND = "remove_friend";
constexpr const char* GET_FRIENDS = "get_friends";

// Статистика
constexpr const char* GET_STATS = "get_stats";

// Приглашения
constexpr const char* INVITE = "invite";
constexpr const char* INVITE_RECEIVED = "invite_received";
constexpr const char* INVITE_RESPONSE = "invite_response";

// Поиск игры
constexpr const char* FIND_GAME = "find_game";
constexpr const char* CANCEL_SEARCH = "cancel_search";
constexpr const char* GAME_STARTED = "game_started";

// Наблюдение
constexpr const char* SPECTATE = "spectate";
constexpr const char* SPECTATE_UPDATE = "spectate_update";

// Игра
constexpr const char* PLACE_SHIPS = "place_ships";
constexpr const char* PLAYER_READY = "player_ready";
constexpr const char* BATTLE_START = "battle_start";
constexpr const char* SHOOT = "shoot";
constexpr const char* OPPONENT_SHOT = "opponent_shot";
constexpr const char* SURRENDER = "surrender";
constexpr const char* GAME_OVER = "game_over";
}  // namespace action

namespace error_code {
// Аутентификация (1xxx)
constexpr int INVALID_NICKNAME = 1001;
constexpr int GUID_NOT_FOUND = 1002;

// Игра (2xxx)
constexpr int GAME_NOT_FOUND = 2001;
constexpr int NOT_YOUR_TURN = 2002;
constexpr int INVALID_PLACEMENT = 2003;
constexpr int CELL_ALREADY_ATTACKED = 2004;
constexpr int TURN_TIMEOUT = 2005;
constexpr int PLAYER_DISCONNECTED = 2006;
constexpr int SPECTATE_FRIENDS_ONLY = 2007;

// Игроки (3xxx)
constexpr int PLAYER_NOT_FOUND = 3001;
constexpr int PLAYER_NOT_ONLINE = 3002;
constexpr int PLAYER_IN_GAME = 3003;

// Приглашения (4xxx)
constexpr int INVITE_NOT_FOUND = 4001;
constexpr int INVITE_EXPIRED = 4002;

// Друзья (5xxx)
constexpr int FRIEND_ALREADY_ADDED = 5001;
constexpr int CANNOT_ADD_SELF = 5002;
}  // namespace error_code

namespace game_over_reason {
constexpr const char* ALL_SHIPS_SUNK = "all_ships_sunk";
constexpr const char* SURRENDER = "surrender";
constexpr const char* DISCONNECT = "disconnect";
constexpr const char* TIMEOUT = "timeout";
}  // namespace game_over_reason

inline std::string errorMessage(int code) {
    switch (code) {
        case error_code::INVALID_NICKNAME:
            return "Недопустимый никнейм";
        case error_code::GUID_NOT_FOUND:
            return "GUID не найден";
        case error_code::GAME_NOT_FOUND:
            return "Игра не найдена";
        case error_code::NOT_YOUR_TURN:
            return "Не ваш ход";
        case error_code::INVALID_PLACEMENT:
            return "Невалидная расстановка кораблей";
        case error_code::CELL_ALREADY_ATTACKED:
            return "Клетка уже атакована";
        case error_code::TURN_TIMEOUT:
            return "Время хода истекло";
        case error_code::PLAYER_DISCONNECTED:
            return "Игрок отключился";
        case error_code::SPECTATE_FRIENDS_ONLY:
            return "Наблюдение доступно только за друзьями";
        case error_code::PLAYER_NOT_FOUND:
            return "Игрок не найден";
        case error_code::PLAYER_NOT_ONLINE:
            return "Игрок не в сети";
        case error_code::PLAYER_IN_GAME:
            return "Игрок уже в игре";
        case error_code::INVITE_NOT_FOUND:
            return "Приглашение не найдено";
        case error_code::INVITE_EXPIRED:
            return "Приглашение истекло";
        case error_code::FRIEND_ALREADY_ADDED:
            return "Друг уже добавлен";
        case error_code::CANNOT_ADD_SELF:
            return "Нельзя добавить себя в друзья";
        default:
            return "Неизвестная ошибка";
    }
}

}  // namespace protocol

}  // namespace seabattle